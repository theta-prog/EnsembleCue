# 技術選定・比較レポート

調査日: 2026-11-03（JST）。公式ページの表示と取得ソースを区別する。「最新」ページは更新されるため、採用依存はタグとcommitで固定する。

## 言語と構成の比較

点数は1〜5の設計上の評価であり、他構成の実測性能ではない。重みは依頼どおり30/25/15/10/10/10%。ドラム解析の難しさはどの言語でも残るため、拍追従の点数は同点とした。

|構成|RT 30|解析 25|統合 15|保守 10|offline 10|許諾/拡張 10|加重点|
|---|---:|---:|---:|---:|---:|---:|---:|
|C++20 / JUCE / 自作DSP / JUCE GUI|5|3|5|4|5|3|4.20|
|Rust / CPAL / 自作DSP / egui|4|3|3|3|5|5|3.70|
|C++ / PortAudio / 自作DSP / Qt|5|3|4|3|5|4|4.05|

C++は既存の音声デバイス・MIDI・音源SDKと直接統合でき、コールバックの所有権と確保を制御できる。Rustは安全性が優れるがCPALだけでは製品に必要なデバイスUI、MIDI、音源を揃えられない。C#はGUIに適するが音声境界でGCとネイティブ相互運用を設計する必要がある。Pythonはモデル研究に適するが、このMVPのコールバックを担当させる理由はない。WebはASIO機器設定と単一サンプルクロック管理の要件に合いにくい。

## 音声I/OとGUI

|候補|調査で確認した版・保守|Windows / license|判断|
|---|---|---|---|
|[JUCE](https://github.com/juce-framework/JUCE/releases/tag/9.0.5)|9.0.5、2026-11-02リリース。取得commit `d60b7a78030661574a615b586d9325ba7a7fa7dd`|ASIO/WASAPI、GUI/MIDI/WAV。モジュールは[AGPLv3 / 商用](https://github.com/juce-framework/JUCE/blob/9.0.5/LICENSE.md)|採用。機器設定まで同一フレームワーク。ライセンス費用とソース公開方針が製品化判断に関係|
|[CPAL](https://github.com/RustAudio/cpal/releases/tag/v0.18.2)|0.18.2。リリース継続|Windows WASAPI、ASIO機能。[Apache-2.0](https://github.com/RustAudio/cpal)|Rust案。入出力クロック整合と機器設定は別途実装・検証が必要|
|[PortAudio](https://github.com/PortAudio/portaudio/releases/tag/v19.7.0)|安定版19.7.0。安定版の古さだけで保守停止とは判定しない|Windows ASIO/WASAPI、[MIT系許諾](https://portaudio.com/license.html)|小さいI/O層として適切。GUI、MIDI等を別途統合|
|[Qt](https://doc.qt.io/qt-6/windows.html)|6.12系列。公式のWindowsページでMSVC 2022対応|Windows 11 x64。[LGPLv3/GPL/商用、モジュール依存](https://doc.qt.io/qt-6.10/licensing.html)|PortAudio案のGUI。MVPではJUCE GUIとの重複を避ける|
|[egui](https://github.com/emilk/egui/releases/tag/0.37.0)|0.37.0、2026-10-15|Windows native、[MIT / Apache-2.0](https://github.com/emilk/egui)|Rust案。即時モードでも音声とは分離可能|
|Win32 native / Web UI|Windows SDK 10.0.26100をローカルで確認。Webは特定framework未選定|OS API / 各依存の許諾に従う|Win32は管理コード増、Webはbridge増。見た目よりデバイス統合を優先|

ASIOはJUCE同梱ヘッダーを使用。ローカルの`native/asio/LICENSE.txt`でSteinberg独自条件またはGPLv3を確認した。この構成はGPLv3側を使用し、JUCE AGPL構成と組み合わせる。SDKの独自条件側での配布と混同しない。

## 拍追従・解析

|候補|版/保守・許諾・対応|メリット / 懸念|
|---|---|---|
|[aubio](https://github.com/aubio/aubio/releases)|0.4.9、GPLv3。Cライブラリ。公式[download](https://aubio.org/download)は古い0.4.7/Windows 0.4.6を案内し、release一覧と不一致|既存onset/tempo実装。Windowsでの現行ビルドと実録音比較を追加する価値があるが、今回その精度を実測したとはしない|
|[Essentia](https://essentia.upf.edu/installing.html)|公式docsは2.1-beta6-dev、GitHubのlatest releaseは古い2.0.1。C++はWindows対応、公式手順はMinGWクロスコンパイルを案内し、そのbinaryはMSVC非互換。Windows Python bindingsは同ページで未対応。[AGPLv3/商用、モデルは別許諾](https://essentia.upf.edu/licensing_information.html)|解析機能が豊富。MSVCへの導入コスト、全曲解析・依存・モデルライセンスと因果的低遅延要件を切り分ける必要|
|自作因果DSP（採用）|本repo v0.1.0 / C++20 / AGPLv3|5 ms特徴、低/中域の立ち上がり、直近6秒の重み付き周期評価、250 ms更新、位相補正。推論モデル不要。合成音源で検証可能だが生ドラムへの一般化は未知|
|[Beat This!](https://github.com/CPJKU/beat_this)|公式ISMIR 2024実装。固定release番号は今回確認できず。コードと公開weightはMIT。Python/PyTorch|学習済みbeat/downbeat推定、録音の比較基準候補。ファイル推論をそのままゼロ先読みのオンライン性能とは扱わない。Windows/GPU構成の動作は未検証|
|[B-Keeper](https://nime.org/proc/nime2007_robertson/)|2007のJava/Max実装論文。現行配布・コードlicenseは未確認|ドラムが主導してsequencerを小さく補正する先行研究。発想を参照し、コードは取り込まない。既存DAW依存を本アプリに持ち込まない|

初期BPMの倍半解釈、強い裏拍、スネアロール、ハイハットだけの演奏、PA回り込みは依然難しい。周波数帯は楽器クラスの教師あり分類ではなく、kick/snare/hatを認識できたとは表示しない。拍の絶対的な小節頭は推定せず、最初の有効オンセットを基準にする。

## 音楽生成

|候補|公式情報・許諾|本件での評価|
|---|---|---|
|ルール・確率生成（採用）|本repo v0.1.0|低い計算負荷、コード制約、モチーフ再利用、決定的seedでテスト可能。学習済みAIほど豊かな表現ではない|
|Music Transformer / MusicVAE系MIDI生成|[Magenta](https://github.com/magenta/magenta)は2026-01-06にarchive、Apache-2.0コード。weightと学習データは個別確認|拍単位の生成契約と相性が良い。条件付け、古い依存、推論時間の分布、音域制約を検証してから追加|
|ローカルLLM|[llama.cpp](https://github.com/ggml-org/llama.cpp)はMIT、更新が頻繁なため未採用commitを固定したとはしない。各modelのlicenseは別|小節単位の提案には使えるが、構造化noteの妥当性と生成遅延が不確実。6GB VRAMでの特定modelの速度は未測定|
|[Magenta RealTime 2](https://github.com/magenta/magenta-realtime)|公式READMEは230M/2.4B、リアルタイム対象はApple Silicon。NVIDIAはPython経由offline説明。[コードApache-2.0、weight CC-BY-4.0](https://github.com/magenta/magenta-realtime/blob/main/MODEL.md)|音声生成として有力だが、本Windows/RTX3050のリアルタイム追従を保証する資料ではない。今回は導入しない|
|ハイブリッド（次段階）|未実装|モデルが候補Phraseを先読み生成→ルールで和声/音域/長さ検査→sample clockで発音。遅延時は現在モチーフを継続。モデルに発音時刻を任せない|

## 音源・出力

|候補|公式情報・ライセンス|判断|
|---|---|---|
|同梱加算合成（採用）|本repo、外部サンプルなし|配布が容易、位相/包絡を直接制御、ベース音色は簡易で実ベース再現ではない|
|[FluidSynth](https://www.fluidsynth.org/)|公式topで2.6.2（2026-11-01）。Windows対応。[LGPL条件](https://www.fluidsynth.org/wiki/LicensingFAQ/)、SoundFontの権利は別|音質向上の次候補。DLL/音源配布とrender APIの実時間挙動を検証後に追加|
|[VST3 SDK](https://github.com/steinbergmedia/vst3sdk)|公式[履歴](https://steinbergmedia.github.io/vst3_dev_portal/pages/Versions/Index.html)で3.8.1（2026-08-11）。[3.8以降MIT](https://steinbergmedia.github.io/vst3_dev_portal/pages/VST%2B3%2BLicensing/Index.html)、Windows対応。未導入|ユーザー所有音源を活用可能。ただしplugin scan/crash/状態保存/ホスト実装がMVPを膨らませるため未採用|
|外部/仮想MIDI|JUCE MidiOutputで既存portへ送信|実装済み。ただし仮想portドライバーは同梱せず、外部音源は別途必要。内部音声よりjitterが増える|

## 技術検証と選定の範囲

採用案について実際にコンパイルし、合成ドラムの60〜180 BPM・ramp・step・shuffle・fill・停止/再開、SPSC、発音重複、古いepoch排除をテストした。WAVの読込/書出しとMIDIの書出しをCLIで検証する。30分音声長のaccelerated処理とデバイスの実時間測定を区別する。

他案のコールバック遅延を同一条件で測定していないため「JUCEはCPALより何ms速い」とは結論しない。今回の選択は、音声処理の制御性が同等に確保できる候補の中で、統合範囲と検証作業を小さくする判断。実測値は[VALIDATION](VALIDATION.md)に記録する。
