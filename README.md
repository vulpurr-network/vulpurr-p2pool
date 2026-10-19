<p align="center"><img src="https://vulpurr.com/logo.svg" width="96" alt="Vulpurr"></p>

<h1 align="center">Vulpurr P2Pool</h1>

<p align="center">Decentralized mining pool for Vulpurr (VUP): no operator, 0% fee, rewards paid directly in the block.<br>
<a href="https://vulpurr.com">Website</a> · <a href="https://t.me/vulpurr">Telegram</a></p>

**English** · [Русский](#русский)

A fork of [P2Pool by SChernykh](https://github.com/SChernykh/p2pool) adapted to the Vulpurr network. The upstream history is kept and the Vulpurr changes sit on top; the original README is in [README.p2pool.md](README.p2pool.md).

Most people never run it by hand: the [Vulpurr wallet](https://github.com/vulpurr-network/vulpurr-gui) starts it with one click (Advanced → Mining → P2Pool).

## What is different

- Vulpurr network parameters, address prefix `Vu` and seed nodes (`seed.vulpurr.com`, `seed2.vulpurr.com`).
- Its own sidechains (`vulpurr`, `vulpurr-mini`, `vulpurr-nano`) that never mix with Monero's, and a lower minimum share difficulty for a small network. The wallet always uses the main `vulpurr` sidechain.
- P2P port 27889 (27888 for mini).
- [`build-win.sh`](build-win.sh) and [`cmake/mingw_gcc.cmake`](cmake/mingw_gcc.cmake) build a static Windows binary with MinGW.

## Running

```bash
git clone --recursive https://github.com/vulpurr-network/vulpurr-p2pool.git
cd vulpurr-p2pool && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)
./p2pool --host 127.0.0.1 --rpc-port 27081 --zmq-port 27083 --wallet YOUR_VU_ADDRESS
```

`vulpurrd` must run with `--zmq-pub tcp://127.0.0.1:27083`. Point XMRig at `127.0.0.1:3333`.

## License

GPL-3.0, like the original P2Pool ([LICENSE](LICENSE)). Huge thanks to SChernykh and the P2Pool contributors.

---

## Русский

Децентрализованный пул для Vulpurr: без оператора, без комиссии, награда приходит прямо в блоке. Форк [P2Pool от SChernykh](https://github.com/SChernykh/p2pool) под сеть Vulpurr: свои параметры сети, префикс адресов `Vu`, сид-ноды и свои цепочки шар, которые не пересекаются с Monero (кошелёк всегда использует основную). P2P-порт 27889.

Обычно руками его запускать не нужно: кошелёк Vulpurr включает P2Pool одной кнопкой (Дополнительно → Майнинг → P2Pool). Лицензия GPL-3.0, как у оригинала.
