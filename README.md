# kobra

![alt tag](https://raw.githubusercontent.com/jasper-sinclair/kobra/main/src/kobra.png)

UCI chess engine

  [![Release][release-badge]][release-link]
  [![Commits][commits-badge]][commits-link]
  ![Downloads][downloads-badge]
  [![License][license-badge]][license-link]
  
features:
- windows
- 64 bit
- c++20
- alpha-beta
- nnue
- bitboards
- hash
- threads

|       |       |
|-------|------ |
|perft 7|kobra_20_64_ja_avx2_zen2.exe|  
|a2a3|1067431060|
|b2b3|1332339750|
|c2c3|1440749440|
|d2d3|2275986920|
|e2e3|3061384100|
|f2f3|1020210080|
|g2g3|1359876510|
|h2h3|1066784230|
|a2a4|1370773370|
|b2b4|1340874760|
|c2c4|1577564430|
|d2d4|2696055990|
|e2e4|3094782630|
|f2f4|1196148410|
|g2g4|1302930180|
|h2h4|1384952900|
|b1a3|1201421440|
|b1c3|1485271610|
|g1f3|1476785540|
|g1h3|1206695250|
|nodes|3195901860|
|time|17.4971|

Kobra's strength & unique play results from the use of an original NNUE evaluation created & trained from millions of Kobra vs Kobra self-play games.
The data used is available here: https://github.com/jasper-sinclair/kobra-data

Kobra NNUE is powered by a custom adaptation of https://github.com/dshawul/nnue-probe

[license-badge]:https://img.shields.io/github/license/jasper-sinclair/kobra?style=for-the-badge&label=license&color=success
[license-link]:https://github.com/jasper-sinclair/kobra/blob/main/LICENSE
[release-badge]:https://img.shields.io/github/v/release/jasper-sinclair/kobra?style=for-the-badge&label=official%20release
[release-link]:https://github.com/jasper-sinclair/kobra/releases/latest
[commits-badge]:https://img.shields.io/github/commits-since/jasper-sinclair/kobra/latest?style=for-the-badge
[commits-link]:https://github.com/jasper-sinclair/kobra/commits/main
[downloads-badge]:https://img.shields.io/github/downloads/jasper-sinclair/kobra/total?color=success&style=for-the-badge
