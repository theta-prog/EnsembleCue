# 第三者依存と再現情報

アプリ独自ソース: AGPL-3.0-only、本文はルートの`LICENSE`。

JUCE 9.0.5:

- 公式: https://github.com/juce-framework/JUCE
- 固定commit: `d60b7a78030661574a615b586d9325ba7a7fa7dd`
- ローカルソース: `third_party/JUCE`
- 条件: `third_party/JUCE/LICENSE.md`（AGPLv3 / JUCE商用。本プロトタイプはAGPL）
- 依存一覧: `third_party/JUCE/JUCE.spdx.json`
- 内部のHarfBuzz、SheenBidi、PNG/JPEG/WebP等にも個別の著作権表示・ライセンスがある。JUCEの全ソースとSBOMを維持する。

ASIOヘッダー:

- JUCE同梱 `modules/juce_audio_devices/native/asio`
- 本文の写し: `docs/ASIO-LICENSE.txt`
- Steinberg条件またはGPLv3。このプロトタイプはGPLv3側を使用。
- メーカーのASIOドライバーは既存インストールを使用し、同梱しない。

音源: 独自の加算合成。外部SoundFont、録音サンプル、学習済みweight、VSTを同梱しない。検証用ドラムWAVも独自合成。

ビルド: MSVC / Windows SDKはシステムに既存の開発ツールを使用し、本repoへ再配布しない。CMakeのJUCE取得にはGitが必要。アプリ実行時はネットワークを使用しない。
