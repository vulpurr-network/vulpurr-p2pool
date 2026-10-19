/*
 * This file is part of the Monero P2Pool <https://github.com/SChernykh/p2pool>
 * Copyright (c) 2021-2026 SChernykh <https://github.com/SChernykh>
 * Portions Copyright (c) 2012-2013 The Cryptonote developers
 * Portions Copyright (c) 2014-2021 The Monero Project
 * Portions Copyright (c) 2021 XMRig <https://github.com/xmrig>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "common.h"
#include "wallet.h"
#include "keccak.h"
#include "crypto.h"

extern "C" {
#include "crypto-ops.h"
}

#include "fcmp_pp_crypto.h"

LOG_CATEGORY(Wallet)

static void write_prefix(uint8_t* out, uint64_t prefix)
{
	for (int i = 0; i < p2pool::Wallet::PREFIX_SIZE; ++i) {
		out[i] = static_cast<uint8_t>((prefix & 0x7F) | ((i + 1 < p2pool::Wallet::PREFIX_SIZE) ? 0x80 : 0));
		prefix >>= 7;
	}
}

static bool read_prefix(const uint8_t* in, uint64_t& prefix)
{
	prefix = 0;
	for (int i = 0; i < p2pool::Wallet::PREFIX_SIZE; ++i) {
		prefix |= static_cast<uint64_t>(in[i] & 0x7F) << (7 * i);
		const bool more = (in[i] & 0x80) != 0;
		if (more != (i + 1 < p2pool::Wallet::PREFIX_SIZE)) {
			return false;
		}
	}
	return true;
}

namespace {

// Vulpurr mainnet only, values from cryptonote_config.h: CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX / CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX
constexpr uint64_t vulpurr_prefix = 0x63ac;
constexpr uint64_t vulpurr_prefix_subaddress = 0x48ac;

constexpr std::array<int, 9> block_sizes{ 0, 2, 3, 5, 6, 7, 9, 10, 11 };
constexpr int num_full_blocks = p2pool::Wallet::ADDRESS_LENGTH / block_sizes.back();
constexpr int last_block_size = p2pool::Wallet::ADDRESS_LENGTH % block_sizes.back();

constexpr int block_sizes_lookup[11] = { 0, -1, 1, 2, -1, 3, 4, 5, -1, 6, 7 };

constexpr char alphabet[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
constexpr size_t alphabet_size = sizeof(alphabet) - 1;

static_assert(alphabet_size == 58, "Check alphabet");

struct ReverseAlphabet
{
	int8_t data[256];
	int num_symbols;

	static constexpr ReverseAlphabet init()
	{
		ReverseAlphabet result = {};

		for (int i = 0; i < 256; ++i) {
			result.data[i] = -1;
		}

		result.num_symbols = 0;
		for (size_t i = 0; i < alphabet_size; ++i) {
			if (result.data[static_cast<uint8_t>(alphabet[i])] < 0) {
				result.data[static_cast<uint8_t>(alphabet[i])] = static_cast<int8_t>(i);
				++result.num_symbols;
			}
		}

		return result;
	}
};

constexpr ReverseAlphabet rev_alphabet = ReverseAlphabet::init();

static_assert(rev_alphabet.num_symbols == 58, "Check alphabet");

} // namespace

namespace p2pool {

Wallet::Wallet(const char* address) : m_prefix(0), m_checksum(0), m_type(NetworkType::Invalid), m_subaddress(false)
{
	if (!decode(address) && address) {
		LOGWARN(1, address << " failed to decode");
	}
}

Wallet::Wallet(const Wallet& w)
{
	operator=(w);
}

Wallet& Wallet::operator=(const Wallet& w)
{
	if (this == &w) {
		return *this;
	}

	m_prefix = w.m_prefix;
	m_keys[0] = w.m_keys[0];
	m_keys[1] = w.m_keys[1];
	m_checksum = w.m_checksum;
	m_type = w.m_type;
	m_subaddress = w.m_subaddress;

	return *this;
}

bool Wallet::decode(const char* address)
{
	m_type = NetworkType::Invalid;

	if (!address || (strlen(address) != ADDRESS_LENGTH)) {
		return false;
	}

	constexpr int last_block_size_index = block_sizes_lookup[last_block_size];

	static_assert(last_block_size_index >= 0, "Check ADDRESS_LENGTH");

	uint8_t data[static_cast<size_t>(num_full_blocks) * sizeof(uint64_t) + last_block_size_index] = {};
	int data_index = 0;

	for (int i = 0; i <= num_full_blocks; ++i) {
		uint64_t num = 0;
		uint64_t order = 1;

		for (int j = ((i < num_full_blocks) ? block_sizes.back() : last_block_size) - 1; j >= 0; --j) {
			const int8_t digit = rev_alphabet.data[static_cast<uint8_t>(address[j])];
			if (digit < 0) {
				return false;
			}

			uint64_t hi;
			const uint64_t tmp = num + umul128(order, static_cast<uint64_t>(digit), &hi);
			if ((tmp < num) || hi) {
				return false;
			}

			num = tmp;
			order *= alphabet_size;
		}

		address += block_sizes.back();

		for (int j = static_cast<int>((i < num_full_blocks) ? sizeof(num) : last_block_size_index) - 1; j >= 0; --j) {
			data[data_index++] = static_cast<uint8_t>(num >> (j * 8));
		}
	}

	if (!read_prefix(data, m_prefix)) {
		return false;
	}

	if (m_prefix == vulpurr_prefix) {
		m_type = NetworkType::Mainnet;
	}
	else if (m_prefix == vulpurr_prefix_subaddress) {
		m_type = NetworkType::Mainnet;
		m_subaddress = true;
	}
	else {
		return false;
	}

	memcpy(m_keys[0].h, data + PREFIX_SIZE, HASH_SIZE);
	memcpy(m_keys[1].h, data + PREFIX_SIZE + HASH_SIZE, HASH_SIZE);
	memcpy(&m_checksum, data + PREFIX_SIZE + HASH_SIZE * 2, sizeof(m_checksum));

	uint8_t md[200];
	keccak(data, sizeof(data) - sizeof(m_checksum), md);

	if (memcmp(&m_checksum, md, sizeof(m_checksum)) != 0) {
		m_type = NetworkType::Invalid;
	}

	ge_p3 point;
	if ((ge_frombytes_vartime(&point, m_keys[0].h) != 0) || (ge_frombytes_vartime(&point, m_keys[1].h) != 0)) {
		m_type = NetworkType::Invalid;
	}

	if (!torsion_check()) {
		LOGWARN(1, "Torsion check failed for wallet " << *this << "! It will not be compatible with FCMP++.");
		m_type = NetworkType::Invalid;
		return false;
	}

	return valid();
}

bool Wallet::assign(const hash& spend_pub_key, const hash& view_pub_key, NetworkType type, bool subaddress)
{
	ge_p3 point;
	if ((ge_frombytes_vartime(&point, spend_pub_key.h) != 0) || (ge_frombytes_vartime(&point, view_pub_key.h) != 0)) {
		return false;
	}

	if (type != NetworkType::Mainnet) {
		return false;
	}
	m_prefix = subaddress ? vulpurr_prefix_subaddress : vulpurr_prefix;

	m_keys[0] = spend_pub_key;
	m_keys[1] = view_pub_key;

	uint8_t data[PREFIX_SIZE + HASH_SIZE * 2];
	write_prefix(data, m_prefix);
	memcpy(data + PREFIX_SIZE, spend_pub_key.h, HASH_SIZE);
	memcpy(data + PREFIX_SIZE + HASH_SIZE, view_pub_key.h, HASH_SIZE);

	uint8_t md[200];
	keccak(data, sizeof(data), md);

	memcpy(&m_checksum, md, sizeof(m_checksum));

	m_type = type;
	m_subaddress = subaddress;

	if (!torsion_check()) {
		LOGWARN(1, "Torsion check failed for wallet " << *this << "! It will not be compatible with FCMP++.");
		// Can't make it invalid here, or it will soft-fork different P2Pool v4.x versions
	}

	return true;
}

void Wallet::encode(char (&buf)[ADDRESS_LENGTH]) const
{
	uint8_t data[PREFIX_SIZE + HASH_SIZE * 2 + sizeof(m_checksum)];

	write_prefix(data, m_prefix);
	memcpy(data + PREFIX_SIZE, m_keys[0].h, HASH_SIZE);
	memcpy(data + PREFIX_SIZE + HASH_SIZE, m_keys[1].h, HASH_SIZE);
	memcpy(data + PREFIX_SIZE + HASH_SIZE * 2, &m_checksum, sizeof(m_checksum));

	for (int i = 0; i <= num_full_blocks; ++i) {
		uint64_t n = 0;
		for (int j = 0; (j < 8) && (i * sizeof(uint64_t) + j < sizeof(data)); ++j) {
			n = (n << 8) | data[i * sizeof(uint64_t) + j];
		}
		for (int j = ((i < num_full_blocks) ? block_sizes.back() : last_block_size) - 1; j >= 0; --j) {
			const int digit = static_cast<int>(n % alphabet_size);
			n /= alphabet_size;
			buf[i * block_sizes.back() + j] = alphabet[digit];
		}
	}
}

bool Wallet::get_eph_public_key(const hash& txkey_sec, size_t output_index, hash& eph_public_key, uint8_t& view_tag, const uint8_t* expected_view_tag) const
{
	hash derivation;
	if (!generate_key_derivation(m_keys[1], txkey_sec, output_index, derivation, view_tag)) {
		return false;
	}

	if (expected_view_tag && (view_tag != *expected_view_tag)) {
		return false;
	}

	if (!derive_public_key(derivation, output_index, m_keys[0], eph_public_key)) {
		return false;
	}

	return true;
}

bool Wallet::torsion_check() const
{
	ge_p3 p1, p2;
	if ((ge_frombytes_vartime(&p1, m_keys[0].h) != 0) || (ge_frombytes_vartime(&p2, m_keys[1].h) != 0)) {
		return false;
	}

	return
		!fcmp_pp::mul8_is_identity(p1) &&
		!fcmp_pp::mul8_is_identity(p2) &&
		fcmp_pp::torsion_check_vartime(p1) &&
		fcmp_pp::torsion_check_vartime(p2);
}

} // namespace p2pool
