# Bundled quote-renderer fonts

The native quote renderer registers these application fonts with Pango before
creating a layout. This makes rendering independent of fonts installed on the
host. All four files are unmodified upstream binaries distributed under the
SIL Open Font License 1.1. The corresponding complete license text is retained
beside each binary.

Pinned Google Fonts revision:
`352f6b7d9d6cc4fa9e242b931291d31b21a6dc84`

| Bundled file | Upstream family/source | License | SHA-256 |
| --- | --- | --- | --- |
| `NotoSans-Variable.ttf` | [Noto Sans](https://github.com/google/fonts/blob/352f6b7d9d6cc4fa9e242b931291d31b21a6dc84/ofl/notosans/NotoSans%5Bwdth%2Cwght%5D.ttf) | `OFL-NotoSans.txt` | `bfb7bb691513f12e734dc346c03a03f784912432d7e3fa8e56efcf906fe86b3d` |
| `NotoSansKR-Variable.ttf` | [Noto Sans KR](https://github.com/google/fonts/blob/352f6b7d9d6cc4fa9e242b931291d31b21a6dc84/ofl/notosanskr/NotoSansKR%5Bwght%5D.ttf) | `OFL-NotoSansKR.txt` | `194018e6b2b293a7964f037b25c0249ce1418bc9ab3c971060a03aa57861e252` |
| `NotoSansArabic-Variable.ttf` | [Noto Sans Arabic](https://github.com/google/fonts/blob/352f6b7d9d6cc4fa9e242b931291d31b21a6dc84/ofl/notosansarabic/NotoSansArabic%5Bwdth%2Cwght%5D.ttf) | `OFL-NotoSansArabic.txt` | `63111b5b2e074dd48cc67692e0a2726d86ee94c1c37fe8598257b7b4e87e869e` |
| `NotoEmoji-Variable.ttf` | [Noto Emoji](https://github.com/google/fonts/blob/352f6b7d9d6cc4fa9e242b931291d31b21a6dc84/ofl/notoemoji/NotoEmoji%5Bwght%5D.ttf) | `OFL-NotoEmoji.txt` | `de6c18832938afc99caf132b39d6a30a19bac7f2e812e28db2535b4608d27551` |

`QuoteEmojiBrand` accepts upstream compatibility names such as Apple, Google,
Twitter, Facebook, Samsung, and JoyPixels. Those selectors never load or imply
redistribution of proprietary artwork: every selector deliberately renders
with the bundled open Noto Emoji fallback.
