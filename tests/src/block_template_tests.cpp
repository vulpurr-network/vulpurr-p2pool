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
#include "side_chain.h"
#include "wallet.h"
#include "keccak.h"
#include "merkle.h"
#include "params.h"
#include "gtest/gtest.h"

namespace p2pool {

static hash H(const char* s)
{
	hash result;
	from_hex(s, strlen(s), result);
	return result;
};

TEST(block_template, update)
{
	thread_pool_init();
	init_crypto_cache();
	{
	SideChain sidechain(nullptr, NetworkType::Mainnet);
	BlockTemplate tpl(&sidechain, nullptr);
	tpl.rng().seed(123);

	MinerData data;
	data.major_version = 16;
	data.height = 2762973;
	data.prev_id = H("81a0260b29d5224e88d04b11faff321fbdc11c4570779386b2a1817a86dc622c");
	data.seed_hash = H("33d0fb381466f04d6a1919ced3b698f54a28add3da5a6479b096c67df7a4974c");
	data.difficulty = { 300346053753ULL, 0 };
	data.median_weight = 300000;
	data.already_generated_coins = 18204981557254756780ULL;
	data.median_timestamp = (1ULL << 35) - 2;

	Mempool mempool;
	Params params;

	params.m_miningWallet = Wallet("VuA6u7NUSZzQsVjMKkZxAdV1hxdCo4nmees727HhMSCRar8e268LKcXXVZ2Rz41XPCdgWS3TquZY74e24hcUYH5V4JtAkfHUWM");

	// Test 1: empty template
	tpl.update(data, mempool, params);
	ASSERT_EQ(tpl.get_reward(), 600000000000ULL);

	const PoolBlock* b = tpl.pool_block_template();
	ASSERT_EQ(b->m_sidechainId, H("8ac07ebf187d3d3c403435813f82beb21b496833e6b39b19ab19f2223ffae678"));

	std::vector<uint8_t> blobs;
	uint64_t height;
	difficulty_type diff, aux_diff, sidechain_diff;
	hash seed_hash;
	size_t nonce_offset;
	uint32_t template_id;
	tpl.get_hashing_blobs(0, 1000, blobs, height, diff, aux_diff, sidechain_diff, seed_hash, nonce_offset, template_id);

	ASSERT_EQ(height, 2762973);
	ASSERT_EQ(diff, 300346053753ULL);
	ASSERT_EQ(sidechain_diff, sidechain.difficulty());
	ASSERT_EQ(seed_hash, data.seed_hash);
	ASSERT_EQ(nonce_offset, 39U);
	ASSERT_EQ(template_id, 1U);

	hash blobs_hash;
	keccak(blobs.data(), static_cast<int>(blobs.size()), blobs_hash.h);
	ASSERT_EQ(blobs_hash, H("078082eab6b8cb2ded5796a037ffe790462d0fed267e47efe9d776ec69cd7f2a"));

	// Test 2: mempool with high fee and low fee transactions, it must choose high fee transactions
	for (uint64_t i = 0; i < 513; ++i) {
		hash h;
		h.u64()[0] = i;

		TxMempoolData tx;
		tx.id = static_cast<indexed_hash>(h);
		tx.fee = (i < 256) ? 30000000 : 60000000;
		tx.weight = 1500;
		mempool.add(tx);
	}
	ASSERT_EQ(mempool.size(), 513);

	// Test transaction removing from mempool
	{
		std::vector<hash> tx_hashes;

		// Empty list, should do nothing
		mempool.remove(tx_hashes);
		ASSERT_EQ(mempool.size(), 513);

		hash h;
		*reinterpret_cast<uint64_t*>(h.h) = 512;
		tx_hashes.push_back(h);

		// Should remove a single hash
		mempool.remove(tx_hashes);
	}
	ASSERT_EQ(mempool.size(), 512);

	tpl.update(data, mempool, params);
	ASSERT_EQ(tpl.get_reward(), 612054770773ULL);

	ASSERT_EQ(b->m_sidechainId, H("09dc0ef44b832fc5d12279308eef9884e520136301954cda9d86fe28cae0715f"));
	ASSERT_EQ(b->m_transactions.size(), 203);

	for (size_t i = 1; i < b->m_transactions.size(); ++i) {
		ASSERT_GE(static_cast<hash>(b->m_transactions[i]).u64()[0], 256);
	}

	tpl.get_hashing_blobs(0, 1000, blobs, height, diff, aux_diff, sidechain_diff, seed_hash, nonce_offset, template_id);

	ASSERT_EQ(height, 2762973);
	ASSERT_EQ(diff, 300346053753ULL);
	ASSERT_EQ(sidechain_diff, sidechain.difficulty());
	ASSERT_EQ(seed_hash, data.seed_hash);
	ASSERT_EQ(nonce_offset, 39U);
	ASSERT_EQ(template_id, 2U);

	keccak(blobs.data(), static_cast<int>(blobs.size()), blobs_hash.h);
	ASSERT_EQ(blobs_hash, H("7c8fa636bf189ad53b34c4817d335059dfa516696e51a173cfe39e8519690ae4"));

	// Test 3: small but not empty mempool, and aux chains

	std::vector<TxMempoolData> transactions;

	for (uint64_t i = 0; i < 10; ++i) {
		hash h;
		h.u64()[0] = i;

		TxMempoolData tx;
		tx.id = static_cast<indexed_hash>(h);
		tx.fee = 30000000;
		tx.weight = 1500;
		transactions.push_back(tx);
	}
	mempool.swap_transactions(transactions);
	ASSERT_EQ(mempool.size(), 10);

	data.aux_chains.emplace_back(H("01f0cf665bd4cd31cbb2b2470236389c483522b350335e10a4a5dca34cb85990"), H("d9de1cfba7cdbd47f12f77addcb39b24c1ae7a16c35372bf28d6aee5d7579ee6"), difficulty_type(1000000));

	// The aux slot depends on the sidechain consensus ID: pick a nonce where the chains don't share a slot
	ASSERT_TRUE(find_aux_nonce({ sidechain.consensus_hash(), data.aux_chains[0].unique_id }, data.aux_nonce));

	tpl.update(data, mempool, params);
	ASSERT_EQ(tpl.get_reward(), 600300000000ULL);

	ASSERT_EQ(b->m_sidechainId, H("a99dde85c81862edf787e5c04bc28991c1180f49e42de099156d01051c961fcc"));
	ASSERT_EQ(b->m_transactions.size(), 11);

	tpl.get_hashing_blobs(0, 1000, blobs, height, diff, aux_diff, sidechain_diff, seed_hash, nonce_offset, template_id);

	ASSERT_EQ(height, 2762973);
	ASSERT_EQ(diff, 300346053753ULL);
	ASSERT_EQ(sidechain_diff, sidechain.difficulty());
	ASSERT_EQ(seed_hash, data.seed_hash);
	ASSERT_EQ(nonce_offset, 39U);
	ASSERT_EQ(template_id, 3U);

	keccak(blobs.data(), static_cast<int>(blobs.size()), blobs_hash.h);
	ASSERT_EQ(blobs_hash, H("db0883d3a29173e80a56c1f542a775b975944cad731340230c2da298f94d8878"));

	// Test 4: mempool with a lot of transactions with various fees, all parts of transaction picking algorithm should be tested

	mempool.clear();

	std::mt19937_64 rng;

	for (uint64_t i = 0; i < 10000; ++i) {
		hash h;
		h.u64()[0] = i;

		TxMempoolData tx;
		tx.id = static_cast<indexed_hash>(h);
		tx.weight = 1500 + (rng() % 10007);
		tx.fee = 30000000 + (rng() % 100000007);

		mempool.add(tx);
	}
	ASSERT_EQ(mempool.size(), 10000);

	tpl.update(data, mempool, params);
	ASSERT_EQ(tpl.get_reward(), 619742028747ULL);

	ASSERT_EQ(b->m_sidechainId, H("9a3bfb3dfd90c28a6500c0eef4af4fe90d7b6f0813da9307f83b0878af01c6a8"));
	ASSERT_EQ(b->m_transactions.size(), 174);

	tpl.get_hashing_blobs(0, 1000, blobs, height, diff, aux_diff, sidechain_diff, seed_hash, nonce_offset, template_id);

	ASSERT_EQ(height, 2762973);
	ASSERT_EQ(diff, 300346053753ULL);
	ASSERT_EQ(sidechain_diff, sidechain.difficulty());
	ASSERT_EQ(seed_hash, data.seed_hash);
	ASSERT_EQ(nonce_offset, 39U);
	ASSERT_EQ(template_id, 4U);

	keccak(blobs.data(), static_cast<int>(blobs.size()), blobs_hash.h);
	ASSERT_EQ(blobs_hash, H("3de3443e80f909dbd6350fda549bf34f21a2c056aba8e7d19f6826adacf546ca"));
	}
	thread_pool_destroy();
	destroy_crypto_cache();

#ifdef WITH_INDEXED_HASHES
	indexed_hash::cleanup_storage();
#endif
}

TEST(block_template, submit_sidechain_block)
{
	thread_pool_init();
	init_crypto_cache();
	{
	SideChain sidechain(nullptr, NetworkType::Mainnet, "unit_test");

	ASSERT_EQ(sidechain.consensus_hash(), H("4a1acc8e4ecebb0d4d2e51ee21e4a650095d6f1f4422e9660a3f7960a4d0a7e2"));

	BlockTemplate tpl(&sidechain, nullptr);
	tpl.rng().seed(123);

	BlockTemplate tpl2(&sidechain, nullptr);
	tpl2.rng().seed(456);

	BlockTemplate tpl3(&sidechain, nullptr);
	tpl3.rng().seed(789);

	MinerData data;
	data.major_version = 16;
	data.height = 2762973;
	data.prev_id = H("81a0260b29d5224e88d04b11faff321fbdc11c4570779386b2a1817a86dc622c");
	data.seed_hash = H("33d0fb381466f04d6a1919ced3b698f54a28add3da5a6479b096c67df7a4974c");
	data.difficulty = { 300346053753ULL, 0 };
	data.median_weight = 300000;
	data.already_generated_coins = 18204981557254756780ULL;
	data.median_timestamp = (1ULL << 35) - (sidechain.chain_window_size() * 2 + 10) * sidechain.block_time() - 3600;

	Mempool mempool;
	Params params;

	params.m_miningWallet = Wallet("VuA6u7NUSZzQsVjMKkZxAdV1hxdCo4nmees727HhMSCRar8e268LKcXXVZ2Rz41XPCdgWS3TquZY74e24hcUYH5V4JtAkfHUWM");

	std::mt19937_64 rng(101112);

	for (uint64_t i = 0, i2 = 0, i3 = 0; i < sidechain.chain_window_size() * 3; ++i) {
		tpl.update(data, mempool, params);

		if ((rng() % 31) == 0) {
			tpl2.update(data, mempool, params);

			if ((rng() % 11) == 0) {
				tpl3.update(data, mempool, params);
				++i3;
				ASSERT_TRUE(tpl3.submit_sidechain_block(i3, 0, 0));
			}

			++i2;
			ASSERT_TRUE(tpl2.submit_sidechain_block(i2, 0, 0));
		}

		ASSERT_TRUE(tpl.submit_sidechain_block(i + 1, 0, 0));
		data.median_timestamp += sidechain.block_time();
	}

	ASSERT_EQ(sidechain.difficulty(), 2187);
	ASSERT_EQ(sidechain.blocksById().size(), 4493);
	ASSERT_TRUE(sidechain.precalcFinished());

	const PoolBlock* tip = sidechain.chainTip();

	ASSERT_TRUE(tip != nullptr);
	ASSERT_TRUE(tip->m_verified);
	ASSERT_FALSE(tip->m_invalid);

	ASSERT_EQ(tip->m_txinGenHeight, data.height);
	ASSERT_EQ(tip->m_sidechainHeight, sidechain.chain_window_size() * 3 - 1);

	ASSERT_EQ(tip->m_sidechainId, H("0f01d55399b7b9ca96bf35dc7266ec77bb151f64044edca5fa8a6f375f95d7e9"));
	}
	thread_pool_destroy();
	destroy_crypto_cache();

#ifdef WITH_INDEXED_HASHES
	indexed_hash::cleanup_storage();
#endif
}

TEST(block_template, genesis_block_max_timestamp)
{
	thread_pool_init();
	init_crypto_cache();
	{
	SideChain sidechain(nullptr, NetworkType::Mainnet, "unit_test");
	ASSERT_EQ(sidechain.consensus_hash(), H("4a1acc8e4ecebb0d4d2e51ee21e4a650095d6f1f4422e9660a3f7960a4d0a7e2"));

	BlockTemplate tpl(&sidechain, nullptr);
	tpl.rng().seed(0);

	MinerData data;
	data.major_version = 16;
	data.height = 2762973;
	data.prev_id = H("81a0260b29d5224e88d04b11faff321fbdc11c4570779386b2a1817a86dc622c");
	data.seed_hash = H("33d0fb381466f04d6a1919ced3b698f54a28add3da5a6479b096c67df7a4974c");
	data.difficulty = { 300346053753ULL, 0 };
	data.median_weight = 300000;
	data.already_generated_coins = 18204981557254756780ULL;
	data.median_timestamp = std::numeric_limits<uint64_t>::max() - 1;

	Mempool mempool;
	Params params;

	params.m_miningWallet = Wallet("VuA6u7NUSZzQsVjMKkZxAdV1hxdCo4nmees727HhMSCRar8e268LKcXXVZ2Rz41XPCdgWS3TquZY74e24hcUYH5V4JtAkfHUWM");

	tpl.update(data, mempool, params);

	ASSERT_TRUE(tpl.submit_sidechain_block(1, 0, 0));
	ASSERT_EQ(sidechain.difficulty(), 1000);

	const PoolBlock* tip = sidechain.chainTip();

	ASSERT_TRUE(tip != nullptr);
	ASSERT_TRUE(tip->m_verified);
	ASSERT_FALSE(tip->m_invalid);

	ASSERT_EQ(tip->m_timestamp, std::numeric_limits<uint64_t>::max());
	ASSERT_EQ(tip->m_txinGenHeight, data.height);
	ASSERT_EQ(tip->m_sidechainHeight, 0);

	ASSERT_EQ(tip->m_sidechainId, H("72f59e589e67142025fa0a7d7ce3e73eec878185ccf73ff2075f2636b5da2304"));
	}
	thread_pool_destroy();
	destroy_crypto_cache();

#ifdef WITH_INDEXED_HASHES
	indexed_hash::cleanup_storage();
#endif
}

}
