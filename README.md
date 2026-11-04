# EnsembleCue — ドラマーに追従するAIベーシスト

Windows 11向けのオフライン・ネイティブアプリです。生ドラムのマイク入力、録音WAV、合成デモを解析し、拍に追従するクリックとベースを生成します。**検証用プロトタイプであり、スタジオでの演奏品質はまだ合格判定していません。**

ベースはコード制約・モチーフ・確率的変化を使うルールベース生成です。学習済み生成AIモデルや固定MIDIループの再生は使用しません。

## すぐ起動する

ビルド済みアプリ:

`build\EnsembleCue_artefacts\Release\EnsembleCue.exe`

またはPowerShellで:

```powershell
.\scripts\run.ps1
```

1. まず入力を **Synthetic drums (demo)** にし、**Start session** を押します。
2. 数拍後に **FOLLOWING** とBPMが表示され、**Following click** が鳴ります。
3. **AI bassist** をONにするとベースが参加します。音量は低めから調整してください。
4. 実ドラムでは **Live microphone**、**ASIO → Yamaha Steinberg USB ASIO**、入力1/2と出力1/2を選びます。詳細は[接続手順](docs/STUDIO.md)。
5. **Load drum WAV** で録音を再生・解析できます。最大10分、左channelを使います。

このPCでGUI起動、デモ入力での追従、ベース切替、停止、UR22CのWindows Audio/ASIO選択を確認しました。同じUR22CをASIOとWASAPIで同時にテストすると、片方のストリームが停止する条件がありました。検証時は本アプリを1つだけ開き、DAWや測定ツールと同じ機器を取り合わないようにしてください。

UR22C ASIOで30分の連続動作を確認済み（合成入力、48 kHz、実際のbuffer 512 frames）。callback停止・締切超過・キュー欠落は0。ドライバーのxrun計数は非対応、物理往復遅延は未測定です。詳しい結果は[検証記録](docs/VALIDATION.md)。

## 実装済み

- 入出力機器・sample rate・buffer選択、入力レベル表示
- 因果的オンセット解析、60〜180 BPM候補、拍位相、信頼度、密度、簡易fill候補
- 開始/停止、テンポ追従、Tap Tempo、Re-sync
- クリック、内蔵ベース音源、既存MIDI出力portへの送信
- Auto/Manual key（minor）、Rock/Funk/Shuffle、密度・変化量・音量
- 2小節先読み、拍座標のイベント再タイミング、生成欠落時のルート音fallback
- WAV入力CLI、伴奏WAV/MIDI/解析CSVの書出し
- ドライバー報告遅延の補償と手動timing trim
- コールバック時間、xrun、キュー欠落、CPU/メモリの測定ツール

Phase 1の動作するプロトタイプとPhase 2のベース生成まで実装しています。Phase 3のギター/ピアノ・高度なセクション認識は未実装です。生ドラム実録音、PA回り込み、物理ループバック、演奏者による自然さの評価は別途必要です。

## ビルド

必要環境: Visual Studio 2022 Build Toolsの「C++によるデスクトップ開発」、Windows SDK、CMake、Git。ローカルではMSVC 19.44 / SDK 10.0.26100を確認しました。GPU・Python・Node.jsはアプリ実行に不要です。

```powershell
.\scripts\build.ps1
```

初回のみJUCE公式リポジトリから9.0.5を取得しcommitを検証します。以後のビルドと実行はオフライン可能。PowerShellの組織ポリシーでスクリプト実行が禁止されている場合は、次のCMakeコマンドをDeveloper PowerShellで実行してください。システムの実行ポリシーを変更する必要はありません。

```powershell
git clone --depth 1 --branch 9.0.5 https://github.com/juce-framework/JUCE.git third_party/JUCE
cmake -S . -B build -A x64
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

JUCE不要のDSPテストだけなら `scripts\build.ps1 -CoreOnly`。別PCでRelease exeを起動する場合はMicrosoft Visual C++ 2015–2022 x64 runtimeが必要です。インストーラー・署名・自動更新は含みません。

## 再現用コマンド

```powershell
# 合成ドラム→伴奏WAV、入力ドラムWAV、MIDI、解析CSV
.\build\drum_render_artefacts\Release\drum_render.exe --demo "$PWD\results\demo-band.wav" 30

# 録音→伴奏（入力と出力は別ファイル）
.\build\drum_render_artefacts\Release\drum_render.exe "C:\audio\drums.wav" "$PWD\results\session.wav"

# 注釈付き合成音源の誤差評価
.\build\Release\quality_benchmark.exe

# 30分音声長を高速処理（実時間の連続動作試験とは別）
.\build\Release\benchmark.exe 1800

# UR22C ASIO、48kHz/128 framesを要求、30分実時間・無音出力
# GUIアプリを終了してから実行する
.\build\audio_probe_artefacts\Release\audio_probe.exe 1800 --asio
```

`audio_probe`はログを`results`へ保存します。引数なしなら30秒・OS既定機器。`--asio`はYamaha Steinberg USB ASIOを指定するこの環境用の測定設定です。機器が要求したbuffer値を採用しない場合もあるため、`audio-device-metrics.csv`の実際の`buffer_frames`を必ず確認してください。要求値を測定値として扱いません。

## ドキュメント

- [技術比較・公式情報・ライセンス](docs/TECHNOLOGY.md)
- [ADR](docs/ADR-001.md) / [アーキテクチャ](docs/ARCHITECTURE.md)
- [測定結果と未検証事項](docs/VALIDATION.md)
- [スタジオ接続・操作](docs/STUDIO.md)
- [既知の課題・ロードマップ](docs/ROADMAP.md)

このプロトタイプのソースはAGPL-3.0-only。JUCE/ASIOを含む第三者コードの条件は[THIRD_PARTY](docs/THIRD_PARTY.md)を参照してください。外部公開や配布操作は行っていません。
