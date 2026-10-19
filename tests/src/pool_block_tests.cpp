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
#include "crypto.h"
#include "block_template.h"
#include "mempool.h"
#include "pool_block.h"
#include "pow_hash.h"
#include "side_chain.h"
#include "p2p_server.h"
#include "keccak.h"
#include "params.h"
#include "gtest/gtest.h"
#include <fstream>

namespace p2pool {

static hash H(const char* s)
{
	hash result;
	from_hex(s, strlen(s), result);
	return result;
};

TEST(pool_block, deserialize)
{
	init_crypto_cache();
	{
	PoolBlock b;
	// Vulpurr main sidechain ("vulpurr"), block.dat is a share from it (main chain height 2001)
	SideChain sidechain(nullptr, NetworkType::Mainnet);

	constexpr uint64_t expected_consensus_id[HASH_SIZE / sizeof(uint64_t)] = {
		0xbaf33906cd87510cull,
		0x83ed36266b6e6341ull,
		0xbc21a7606064681full,
		0x3804f3e34f300f28ull
	};

	const std::vector<uint8_t>& consensus_id = sidechain.consensus_id();
	ASSERT_EQ(consensus_id.size(), HASH_SIZE);
	ASSERT_EQ(memcmp(consensus_id.data(), expected_consensus_id, HASH_SIZE), 0);

	std::ifstream f("block.dat", std::ios::binary | std::ios::ate);
	ASSERT_EQ(f.good() && f.is_open(), true);

	std::vector<uint8_t> buf(f.tellg());
	f.seekg(0);
	f.read(reinterpret_cast<char*>(buf.data()), buf.size());
	ASSERT_EQ(f.good(), true);

	ASSERT_EQ(b.deserialize(buf.data(), buf.size(), sidechain, false, false), 0);

	{
		const PoolBlock::full_id id = b.get_full_id();

		ASSERT_EQ(memcmp(id.data(), b.m_sidechainId.h, HASH_SIZE), 0);
		ASSERT_EQ(memcmp(id.data() + HASH_SIZE, &b.m_nonce, NONCE_SIZE), 0);
		ASSERT_EQ(memcmp(id.data() + HASH_SIZE + NONCE_SIZE, &b.m_extraNonce, EXTRA_NONCE_SIZE), 0);
	}

	ASSERT_EQ(b.get_payout(Wallet("VuA6fvjxfWV9zgCHNTz3Se2RFYpDLfKt4DffQpS9cdCyP59ymnDRpszavXZ2HMHHYiKrghU1K18W3F8S1VK57BBG5CssTjjYQn")), 35050409908221U);
	ASSERT_EQ(b.get_payout(Wallet("VuA6u7NUSZzQsVjMKkZxAdV1hxdCo4nmees727HhMSCRar8e268LKcXXVZ2Rz41XPCdgWS3TquZY74e24hcUYH5V4JtAkfHUWM")), 0U);
	ASSERT_EQ(b.get_payout(Wallet("VuA6viSJ4AtF7wPZzMU8iubpuTct1v8HFKk25eA3g5pSEzaXHzzNceMCsT4azsUuxnbvpQbpxdRfTXMR35MfvYez2Fr4q1ioSh")), 0U);

	size_t header_size, miner_tx_size;
	int outputs_offset, outputs_blob_size;
	const std::vector<uint8_t> mainchain_data = b.serialize_mainchain_data(&header_size, &miner_tx_size, &outputs_offset, &outputs_blob_size);
	const std::vector<uint8_t> sidechain_data = b.serialize_sidechain_data();

	ASSERT_EQ(mainchain_data.size(), 169U);
	ASSERT_EQ(header_size, 43U);
	ASSERT_EQ(miner_tx_size, 125U);
	ASSERT_EQ(outputs_offset, 50);
	ASSERT_EQ(outputs_blob_size, 42);

	ASSERT_EQ(b.m_majorVersion, 16U);
	ASSERT_EQ(b.m_minorVersion, 16U);
	ASSERT_EQ(b.m_timestamp, 1791804853U);
	ASSERT_EQ(b.m_nonce, 4294947193U);
	ASSERT_EQ(b.m_txinGenHeight, 2001U);
	ASSERT_EQ(b.m_ephPublicKeys.size(), 1U);
	ASSERT_EQ(b.m_outputAmounts.size(), 1U);
	ASSERT_EQ(b.m_extraNonceSize, 4U);
	ASSERT_EQ(b.m_extraNonce, 781038648U);
	ASSERT_EQ(b.m_transactions.size(), 1U);
	ASSERT_EQ(b.m_uncles.size(), 0U);
	ASSERT_EQ(b.m_sidechainHeight, 1937U);
	ASSERT_EQ(b.m_difficulty.lo, 64065U);
	ASSERT_EQ(b.m_difficulty.hi, 0U);
	ASSERT_EQ(b.m_cumulativeDifficulty.lo, 127354550ull);
	ASSERT_EQ(b.m_cumulativeDifficulty.hi, 0U);
	ASSERT_EQ(b.m_depth, 0U);
	ASSERT_EQ(b.m_verified, false);
	ASSERT_EQ(b.m_invalid, false);
	ASSERT_EQ(b.m_broadcasted, false);
	ASSERT_EQ(b.m_wantBroadcast, false);

	// Seed hash for main chain height 2001 (the genesis block)
	const hash seed = H("d39674a0763c5d130a80c5bfdcb635c2dbf8377b294611d8c0270b192c3f890e");

	RandomX_Hasher hasher(nullptr);
	hasher.set_seed(seed);

	hash pow_hash;
	ASSERT_EQ(b.get_pow_hash(&hasher, 0, seed, pow_hash, false, RandomX_Hasher_Base::VM_LANE_P2P), true);

	std::stringstream s;
	s << pow_hash;
	ASSERT_EQ(s.str(), "658fee187198ff2711594c37e88e9dfaf74febc641a0dd603fab47668b320000");

	ASSERT_EQ(b.m_difficulty.check_pow(pow_hash), true);

	// Test self-assignment
	b = b;

	ASSERT_EQ(b.serialize_mainchain_data(), mainchain_data);
	ASSERT_EQ(b.serialize_sidechain_data(), sidechain_data);
	}
	destroy_crypto_cache();

#ifdef WITH_INDEXED_HASHES
	indexed_hash::cleanup_storage();
#endif
}

TEST(pool_block, verify)
{
	thread_pool_init();
	init_crypto_cache();
	{
	struct STest
	{
		const char* m_poolName;
		const char* m_fileName;
		uint64_t m_txinGenHeight;
		uint64_t m_sidechainHeight;
		uint32_t m_expectedSharesNextBlock;
		bool m_shuffle;
		hash m_templateBlobsHash;
	} tests[2] = {
		// Vulpurr main sidechain from genesis up to height 1937 (main chain height 2001)
		{ nullptr, "sidechain_dump.dat", 2001, 1937, 2, false, H("33acbbc11b976d742d6a5452deca7fecf6c6073c6301ae6538ca0f14e21fde7d") },
		{ nullptr, "sidechain_dump.dat", 2001, 1937, 2, true, H("33acbbc11b976d742d6a5452deca7fecf6c6073c6301ae6538ca0f14e21fde7d") },
	};

	for (const STest& t : tests)
	{
		SideChain sidechain(nullptr, NetworkType::Mainnet, t.m_poolName);

		// Difficulty of block 2001 (its seed block is the genesis block with difficulty 1)
		sidechain.m_testMainChainDiff = difficulty_type(2635973ULL, 0ULL);

		std::ifstream f(t.m_fileName, std::ios::binary | std::ios::ate);
		ASSERT_EQ(f.good() && f.is_open(), true);

		std::vector<uint8_t> buf(f.tellg());
		f.seekg(0);
		f.read(reinterpret_cast<char*>(buf.data()), buf.size());
		ASSERT_EQ(f.good(), true);

		std::vector<PoolBlock*> blocks;
		for (const uint8_t *p = buf.data(), *e = buf.data() + buf.size(); p < e;) {
			ASSERT_TRUE(p + sizeof(uint32_t) <= e);
			const uint32_t n = *reinterpret_cast<const uint32_t*>(p);
			p += sizeof(uint32_t);

			ASSERT_TRUE(p + n <= e);

			PoolBlock* b = new PoolBlock();
			ASSERT_EQ(b->deserialize(p, n, sidechain, false, false), 0);
			p += n;

			blocks.push_back(b);
		}

		if (t.m_shuffle) {
			std::mt19937_64 rng;

			for (uint64_t i = 0, k, n = blocks.size(); i < n - 1; ++i) {
				umul128(rng(), n - i, &k);
				std::swap(blocks[i], blocks[i + k]);
			}
		}

		for (uint64_t i = 0, n = blocks.size(); i < n; ++i) {
			ASSERT_TRUE(sidechain.add_block(*blocks[i]));
			ASSERT_TRUE(sidechain.find_block(blocks[i]->m_sidechainId) != nullptr);
			delete blocks[i];
		}

		for (auto it = sidechain.blocksById().begin(); it != sidechain.blocksById().end(); ++it) {
			const PoolBlock* b = it->second;
			ASSERT_TRUE(b->m_verified);
			ASSERT_FALSE(b->m_invalid);
		}

		const PoolBlock* tip = sidechain.chainTip();
		ASSERT_TRUE(tip != nullptr);
		ASSERT_TRUE(tip->m_verified);
		ASSERT_FALSE(tip->m_invalid);

		ASSERT_EQ(tip->m_txinGenHeight, t.m_txinGenHeight);
		ASSERT_EQ(tip->m_sidechainHeight, t.m_sidechainHeight);

		{
			BlockTemplate tpl(&sidechain, nullptr);
			auto& r = tpl.rng();
			r.seed(0);

			MinerData data;
			data.major_version = 16;
			data.height = t.m_txinGenHeight;
			data.prev_id = H("d4b120a9380c71b925a29d538567255ccb252b3ffff99b6af6ed01ad85ced90b");
			data.seed_hash = H("d39674a0763c5d130a80c5bfdcb635c2dbf8377b294611d8c0270b192c3f890e");
			data.difficulty = { 2635973ULL, 0 };
			data.median_weight = 300000;
			data.already_generated_coins = std::numeric_limits<uint64_t>::max();
			data.median_timestamp = (1ULL << 35) - 2;

			Mempool mempool;

			for (uint64_t i = 0; i < 8192; ++i) {
				hash h;
				h.u64()[0] = i;

				TxMempoolData tx;
				tx.id = static_cast<indexed_hash>(h);
				tx.fee = (r() % 1'000'000'000) + 30'000'000;
				tx.weight = (r() % 20'000) + 1'500;
				mempool.add(tx);
			}

			Params params;

			params.m_miningWallet = Wallet("VuA6u7NUSZzQsVjMKkZxAdV1hxdCo4nmees727HhMSCRar8e268LKcXXVZ2Rz41XPCdgWS3TquZY74e24hcUYH5V4JtAkfHUWM");
			params.m_subaddress = Wallet("Vs7QnQWuht41bX8B5jy3PEPGMXK33H3RABgyNSESVST5EfqPeVsU1PCFgY15mMvR8JM1TU2AMC9s3V87cDunSwVN5fBbSdrK6U");

			tpl.update(data, mempool, params);

			std::vector<uint8_t> blobs;
			uint64_t height;
			difficulty_type diff, aux_diff, sidechain_diff;
			hash seed_hash;
			size_t nonce_offset;
			uint32_t template_id;
			const uint32_t blob_size = tpl.get_hashing_blobs(0, 1000, blobs, height, diff, aux_diff, sidechain_diff, seed_hash, nonce_offset, template_id);
			ASSERT_EQ(blob_size, 77);

			hash blobs_hash;
			keccak(blobs.data(), static_cast<int>(blobs.size()), blobs_hash.h);
			ASSERT_EQ(blobs_hash, t.m_templateBlobsHash);
		}

		PoolBlock block;
		ASSERT_TRUE(block.m_minerWallet.decode("VuA6u7NUSZzQsVjMKkZxAdV1hxdCo4nmees727HhMSCRar8e268LKcXXVZ2Rz41XPCdgWS3TquZY74e24hcUYH5V4JtAkfHUWM"));

		std::vector<MinerShare> shares;

		ASSERT_TRUE(sidechain.fill_sidechain_data(block, shares));

		ASSERT_EQ(block.m_sidechainHeight, t.m_sidechainHeight + 1);
		ASSERT_EQ(shares.size(), t.m_expectedSharesNextBlock);

		const PoolBlock* parent = sidechain.find_block(tip->m_parent);
		ASSERT_TRUE(parent != nullptr);

		// Check pruned and compact broadcast blobs

		auto tip_full_blob = tip->serialize_mainchain_data();
		auto v2 = tip->serialize_sidechain_data();
		tip_full_blob.insert(tip_full_blob.end(), v2.begin(), v2.end());

		P2PServer::Broadcast broadcast(*tip, parent);

		{
			PoolBlock block2;
			ASSERT_EQ(block2.deserialize(broadcast.pruned_blob.data(), broadcast.pruned_blob.size(), sidechain, false, true), 0);

			auto v1 = block2.serialize_mainchain_data();
			v2 = block2.serialize_sidechain_data();
			v1.insert(v1.end(), v2.begin(), v2.end());

			ASSERT_EQ(v1, tip_full_blob);

			ASSERT_NE(block2.deserialize(broadcast.pruned_blob.data(), broadcast.pruned_blob.size(), sidechain, false, false), 0);
		}

		if (!broadcast.compact_blob.empty()) {
			PoolBlock block3;
			ASSERT_EQ(block3.deserialize(broadcast.compact_blob.data(), broadcast.compact_blob.size(), sidechain, true, true), 0);

			auto v1 = block3.serialize_mainchain_data();
			v2 = block3.serialize_sidechain_data();
			v1.insert(v1.end(), v2.begin(), v2.end());

			ASSERT_EQ(v1, tip_full_blob);
		}

		if (!broadcast.compact_unpruned_blob.empty()) {
			PoolBlock block4;
			ASSERT_EQ(block4.deserialize(broadcast.compact_unpruned_blob.data(), broadcast.compact_unpruned_blob.size(), sidechain, true, false), 0);

			auto v1 = block4.serialize_mainchain_data();
			v2 = block4.serialize_sidechain_data();
			v1.insert(v1.end(), v2.begin(), v2.end());

			ASSERT_EQ(v1, tip_full_blob);
		}
	}
	}
	thread_pool_destroy();
	destroy_crypto_cache();

#ifdef WITH_INDEXED_HASHES
	indexed_hash::cleanup_storage();
#endif
}

}
