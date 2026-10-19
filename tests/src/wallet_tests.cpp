/*
 * This file is part of the Monero P2Pool <https://github.com/SChernykh/p2pool>
 * Copyright (c) 2021-2024 SChernykh <https://github.com/SChernykh>
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
#include "gtest/gtest.h"

namespace p2pool {

// Vulpurr addresses: 3-byte varint prefix (0x63ac "Vu", subaddresses 0x48ac "Vs"), 98 characters.
// Test vectors reuse the public keys from the upstream Monero tests, encoded with Vulpurr prefixes
// by an independent implementation of keccak and base58.

TEST(wallet, input_output)
{
	// No data
	{
		Wallet w(nullptr);
		ASSERT_FALSE(w.valid());
	}

	// Wrong length
	{
		Wallet w("456");
		ASSERT_FALSE(w.valid());
	}

	// Symbol '0' is not from base-58
	{
		Wallet w("V0A6viSJ4AtF7wPZzMU8iubpuTct1v8HFKk25eA3g5pSEzaXHzzNceMCsT4azsUuxnbvpQbpxdRfTXMR35MfvYez2Fr4q1ioSh");
		ASSERT_FALSE(w.valid());
	}

	// Invalid checksum
	{
		Wallet w("VuA6viSJ4AtF7wPZzMU8iubpuTct1v8HFKk25eA3g5pSEzaXHzzNceMCsT4azsUuxnbvpQbpxdRfTXMR35MfvYez2Fr4q1ioSi");
		ASSERT_FALSE(w.valid());
	}

	// 64-bit overflow
	{
		Wallet w("VuA6viSJ4AtzzzzzzzzzzzbpuTct1v8HFKk25eA3g5pSEzaXHzzNceMCsT4azsUuxnbvpQbpxdRfTXMR35MfvYez2Fr4q1ioSh");
		ASSERT_FALSE(w.valid());
	}

	// Invalid pubkey (valid checksum)
	{
		Wallet w("VuA6wo8ooPuL4NDEuTLx2U9DuKtgePLwHBSKyccaUNh3QdVYd4zdXh3CsT4azsUuxnbvpQbpxdRfTXMR35MfvYez2Fr4pKG4vo");
		ASSERT_FALSE(w.valid());
	}

	// Invalid pubkeys
	{
		Wallet w(nullptr);

		constexpr hash invalid_spend_pub = keccak("invalid spend pub key 1");
		constexpr hash valid_spend_pub = keccak("valid spend pub key 2");

		constexpr hash invalid_view_pub = keccak("invalid view pub key 3");
		constexpr hash valid_view_pub = keccak("valid view pub key 1");

		ASSERT_TRUE(w.assign(valid_spend_pub, valid_view_pub, NetworkType::Mainnet, false));
		ASSERT_FALSE(w.assign(invalid_spend_pub, valid_view_pub, NetworkType::Mainnet, false));
		ASSERT_FALSE(w.assign(valid_spend_pub, invalid_view_pub, NetworkType::Mainnet, false));
		ASSERT_FALSE(w.assign(invalid_spend_pub, invalid_view_pub, NetworkType::Mainnet, false));
	}

	// Invalid prefix (valid checksum)
	{
		Wallet w("W4rmwXFnfSQF7wPZzMU8iubpuTct1v8HFKk25eA3g5pSEzaXHzzNceMCsT4azsUuxnbvpQbpxdRfTXMR35MfvYez2Fr4m9qssP");
		ASSERT_FALSE(w.valid());
	}

	// Monero addresses are not accepted
	{
		Wallet w1("49ccoSmrBTPJd5yf8VYCULh4J5rHQaXP1TeC8Cnqhd5H9Y2cMwkJ9w42euLmMghKtCiQcgZEiGYW1K6Ae4biZ7w1HLSexS6");
		Wallet w2("86eQxzSW4AZfvsWRSop755WZUsog6L3x32NRZukeeShnS4mBGVpcqQhS6pCNxj44usPKNwesZ45ooHyjDku6nVZdT3Q9qrz");

		ASSERT_FALSE(w1.valid());
		ASSERT_FALSE(w2.valid());
	}

	// Only mainnet exists
	{
		Wallet w("VuA6viSJ4AtF7wPZzMU8iubpuTct1v8HFKk25eA3g5pSEzaXHzzNceMCsT4azsUuxnbvpQbpxdRfTXMR35MfvYez2Fr4q1ioSh");
		ASSERT_TRUE(w.valid());

		Wallet w2(nullptr);
		ASSERT_FALSE(w2.assign(w.spend_public_key(), w.view_public_key(), NetworkType::Testnet, false));
		ASSERT_FALSE(w2.assign(w.spend_public_key(), w.view_public_key(), NetworkType::Stagenet, false));
		ASSERT_FALSE(w2.assign(w.spend_public_key(), w.view_public_key(), NetworkType::Invalid, false));
	}

	auto check = [](NetworkType t, bool subaddress, uint64_t prefix, uint32_t checksum, const char* address, const char* spendkey, const char* viewkey)
	{
		// Test Wallet::decode()
		Wallet w(address);
		ASSERT_TRUE(w.valid());
		ASSERT_EQ(w.get_type(), t);
		ASSERT_EQ(w.is_subaddress(), subaddress);
		ASSERT_EQ(w.prefix(), prefix);
		ASSERT_EQ(w.checksum(), checksum);

		char buf[log::Stream::BUF_SIZE + 1];
		log::Stream s(buf);

		s << w.spend_public_key();
		ASSERT_EQ(memcmp(buf, spendkey, HASH_SIZE * 2), 0);

		s.m_pos = 0;
		s << w.view_public_key();
		ASSERT_EQ(memcmp(buf, viewkey, HASH_SIZE * 2), 0);

		// Test Wallet copy
		Wallet w1(w);

		ASSERT_EQ(w1.prefix(),           w.prefix());
		ASSERT_EQ(w1.spend_public_key(), w.spend_public_key());
		ASSERT_EQ(w1.view_public_key(),  w.view_public_key());
		ASSERT_EQ(w1.checksum(),         w.checksum());
		ASSERT_EQ(w1.get_type(),         w.get_type());
		ASSERT_EQ(w1.is_subaddress(),    w.is_subaddress());

		// Test Wallet::assign()
		Wallet w2(nullptr);
		ASSERT_TRUE(w2.assign(w.spend_public_key(), w.view_public_key(), w.get_type(), w.is_subaddress()));

		ASSERT_EQ(w2.prefix(),           w.prefix());
		ASSERT_EQ(w2.spend_public_key(), w.spend_public_key());
		ASSERT_EQ(w2.view_public_key(),  w.view_public_key());
		ASSERT_EQ(w2.checksum(),         w.checksum());
		ASSERT_EQ(w2.get_type(),         w.get_type());
		ASSERT_EQ(w2.is_subaddress(),    w.is_subaddress());

		// Test Wallet::encode()
		const std::string s0 = address;
		const std::string s1 = w.encode();
		const std::string s2 = w2.encode();
		ASSERT_EQ(s1, s0);
		ASSERT_EQ(s2, s0);

		// Test Wallet::encode(buf)
		char buf1[Wallet::ADDRESS_LENGTH] = {};
		w.encode(buf1);
		ASSERT_EQ(memcmp(buf1, address, Wallet::ADDRESS_LENGTH), 0);

		char buf2[Wallet::ADDRESS_LENGTH] = {};
		w2.encode(buf2);
		ASSERT_EQ(memcmp(buf2, address, Wallet::ADDRESS_LENGTH), 0);
	};

	// Correct addresses
	check(
		NetworkType::Mainnet, false, 0x63ac, 0xF2DDA6DCUL, "VuA6viSJ4AtF7wPZzMU8iubpuTct1v8HFKk25eA3g5pSEzaXHzzNceMCsT4azsUuxnbvpQbpxdRfTXMR35MfvYez2Fr4q1ioSh",
		"d2e232e441546a695b27187692d035ef7be5c54692700c9f470dcd706753a833", "06f68970da46f709e2b4d0ffabd0d1f78ea6717786b5766c25c259111f212490"
	);

	check(
		NetworkType::Mainnet, false, 0x63ac, 0xED3D166AUL, "VuA6hsB3rr2BCmQ7wRMiJg1aB5JezFGwV3jNC6yhNxJ2ZweSQJCyvFT8kGRnNajHYxdnVMZKPnhCgT49KTwuqvS1A4mx11rDuE",
		"60fe176eaf3cffb63df130bc25036b661b947900941052fffe6ff4b51fc4f2c5", "9387910b0a2e4f62c32621b77ddbeb3d6c0054e5ed9bc492d87bab1a1eef366d"
	);

	check(
		NetworkType::Mainnet, false, 0x63ac, 0x98F9C8D8UL, "VuA6cK2MhYGRxz4MivULstSBgyX1CYskWfmzdr2UdAyYN3yk2PPg2FX7DxxkYRVUwvgCTukVC39BzXw72wyYfi315PtCcn3yoD",
		"2fc2f902659541e50753853ddb96912baf55f26bebe7d338b5c2239c437ddb98", "b814951166253543cfb0e1b8bdea58f366de824fddb8ef6f895fcf631873f6e1"
	);

	// Address made by vulpurr-wallet-cli
	check(
		NetworkType::Mainnet, false, 0x63ac, 0xF22CE4B5UL, "VuA6u7NUSZzQsVjMKkZxAdV1hxdCo4nmees727HhMSCRar8e268LKcXXVZ2Rz41XPCdgWS3TquZY74e24hcUYH5V4JtAkfHUWM",
		"c4a948a0c58eb68a6643a112daa7794ea1e76ec929e25f8b310a8318baca5bcb", "c96272fcdcb64d29f71a69778fdb4d66676fb736ec15c06cbc7cf6dc2c574d09"
	);

	// Correct subaddresses
	check(
		NetworkType::Mainnet, true, 0x48ac, 0xCDEDFCC5UL, "Vs7QnQWuht41bX8B5jy3PEPGMXK33H3RABgyNSESVST5EfqPeVsU1PCFgY15mMvR8JM1TU2AMC9s3V87cDunSwVN5fBbSdrK6U",
		"6ea1b3ea41038ee8bd793d4d31851cb0ba4e4605213fe8082fdb51393c51b995", "da5d41e01f57c6961083bd6a0f779e856c8054e7cea82265815714b0bd7aece6"
	);

	check(
		NetworkType::Mainnet, true, 0x48ac, 0xF87133F8UL, "Vs7QwPsoGj9eUfZ4WPgJdsiSUjNxiWXymYsGhounrWZK3eRG49iC7ev27wBsJCYSc4X6faHdUS2wUD3tqdfnCrkV7thcGiarQ7",
		"be4e4f17b8e00f69e661683116f7bfcf29b2c96fccbe84ce4ede5e58de0fd074", "43ecfeedf706b181fd5e5a321db3f13632df743113480aa6532973bf06b5da26"
	);
}

}
