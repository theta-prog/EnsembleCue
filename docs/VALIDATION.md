# 検証記録 — 2026-11-03

## 判定の範囲

Phase 1/2のWindowsプロトタイプをビルドし、合成音源・実デバイスI/O・GUIの動作を検証した。**生ドラムの演奏品質について合格とは判定しない。** 自動テストの148,046 assertionsの大半はSPSCの順序と音声値の有限性の反復チェックであり、148,046種類の音楽条件を試した意味ではない。

環境: Intel Core i7-12650H、10 cores/16 logical processors、Windows 11 Pro build 26300、MSVC 19.44、Yamaha Steinberg USB Driver DLL 2.1.6。GPUは使わない。環境取得ログは`environment.txt`、driverのファイル版は`driver-files.txt`。長時間試験の途中に別ディレクトリでクリーンビルドとASanテストを実行しているため、完全な無負荷PCでの測定ではない。

## 合成入力の拍追従

48 kHz、5 ms hop、240-sample特徴、Release/x64。kick=70 Hz、snare=280 Hz、hat=7 kHzの減衰sinを合成。実録音のスネア・シンバル・部屋鳴りを再現したモデルではない。

`tests/Quality.cpp`で入力の正解beat時刻から評価。最初の6秒を除外し、拍内位相の絶対誤差を集計。小節頭の正しさは測らない。clock誤差もソフトウェア上の発音クロックであり、USB/DAC/PAの物理誤差ではない。

|条件|BPM MAE|Tracker位相p50 / p95 (ms)|出力clock位相p95 (ms)|結果|
|---|---:|---:|---:|---|
|一定60/90/120/150/180|0〜0.003|約3.7〜5.0 / 4.1〜5.0|約3.9〜5.0|取得後安定|
|16分刻み120|0.000|4.979 / 4.979|4.979|合成条件では安定|
|Shuffle100|0.000|4.979 / 4.979|4.979|合成条件では安定|
|100→130、30秒ramp|2.617|21.438 / 42.249|49.217|追従するが遅れが残る|
|90→130、12秒でstep|7.100|3.722 / 183.845|184.571|最終130へ復帰。遷移中の大誤差あり|
|2小節末のfill|0.000|4.979 / 4.979|4.979|この合成fillではtempo維持|
|±9 ms microtiming|2.679|3.657 / 9.337|9.842|拍の細かな揺れを平滑化|
|別tempo140の弱いbass混入0.1|0.000|4.979 / 4.979|4.979|この混合条件では120維持|
|同じbass混入0.8|20.007|128.279 / 237.635|238.396|**140に誤ロック、失敗**|

一定tempoの最初のlockは約1.73秒。rampは約2.24秒。初期位相補正の改善前後でrampのTracker位相p50が約65→21 msに改善した（`quality-baseline.csv`と`quality.csv`）。履歴を短くする案はstep条件を悪化させたため採用しなかった。

microtimingのBPM正解は隣接beat間隔から計算するため、±9 msの揺れでも瞬間BPMは変化する。MAEだけでグルーヴの良さを判定しない。confidenceは校正済み確率ではない。強い回り込みで高いconfidenceの誤ロックが起こり得る。

## 自動テスト

`core-tests.txt`に実行ログ。60/75/90/110/120/150/180 × 8分/16分、shuffle、ramp、step、fill、±9 ms、無音、停止/再開、Tap、SPSC並行順序、sample clock、重複note、途中参加、stale epoch、生成starvation fallback、有限出力を確認。

同じコアテストをMSVC AddressSanitizer付きでも実行し、148,046 assertionsが通った。ログは`asan-tests.txt`。これはデータ競合検出器ではなく、JUCE/ASIOドライバー全体のメモリ安全性を証明するものでもない。

別の`build-clean`ディレクトリでGUI/CLIもゼロからビルドし、コアテスト合格を確認（`clean-build.log`、`clean-ctest.txt`）。追加の`sample_rate_tests`は44.1/48/88.2/96/192 kHzで合成120 BPMを解析し、119.998〜120.000 BPMとなった。最終CTestは2/2合格（`ctest.txt`）。これはUR22Cが全sample rateで実測済みという意味ではなく、DSPのsample rate依存を確認したもの。

WAV再入力: `demo-band-drums.wav`を`drum_render`へ再入力し、最終BPMは合成直接入力と同じ119.026。24bit量子化があるためファイルのbyte一致は要求しない。MIDIは960 PPQ、500,000 us/quarter固定tempoに実秒を換算し、演奏時刻を保存する（人間のtempo mapそのものではない）。

`scripts/check-render.mjs`で独立にRIFF chunkとMIDI eventを読み取り検証。30秒、peak=0.2461、RMS=0.0820、再入力との差の最大値=1.192e-7、Note On/Off=76/76。`render-check.json`を保存。Node.jsはこの検証スクリプトだけの任意依存で、製品エンジンには使用しない。

## 30分音声長の高速処理

`benchmark-1800s.csv`は1800秒相当の合成入力を高速処理した結果。128 samplesごとのCore処理時間を計測し、入力fixture生成は計測区間外。Tracker/生成も同じ計測区間に含むため、実アプリのaudio callbackの数値ではない。OSのpreemptionを含む最大時間は外れ値を除外しない。

BPM MAE=0、拍位相p50/p95=4.979 ms（一定120 BPM、8秒warmup除外）。CPU壁時計・処理時間分布はCSVの最終実行値を参照。これは30分間実際に音声デバイスを駆動した試験ではない。

## デバイス・GUI試験

GUIで入力切替、Start/Stop、ベース切替、UR22C Windows AudioおよびYamaha Steinberg USB ASIOの選択、FOLLOWINGを確認。ASIO GUI試験は48 kHz/512 frames。入力は合成、master volume=0。

最初の30秒Windows Audio試験: 48 kHz/480 frames、callback平均90.8 us/最大317.4 us、deadline超過0、feature drop0。driver xrun=1は開始前を引いていない値で、startupか実行中か区別できない。この初回ビルドはdemo monitorにmaster gainが効かない不具合があり、完全無音ではなかった。最終版で修正済み。

次のWindows Audio連続試験は、GUIから同じUR22CをASIOで開いた時点でcallbackが止まった。**失敗・中断扱い**。`audio-probe-interrupted-by-asio.csv`に保存。単に最後のlocked状態を見ると停止を見逃すため、最終測定ツールには毎秒のcallback count増加監視を追加した。

最終版のASIO単独試験は**1800秒完走、終了コード0**。`audio-probe.csv`、`audio-probe-summary.txt`、`audio-device-metrics.csv`、`audio-probe-statistics.json`に保存。入力は合成ドラム、ベース生成/音声合成は実行、master gain=0。外部MIDI機器は接続していない。

|測定項目|実測結果|
|---|---:|
|デバイス/backend|Yamaha Steinberg USB ASIO / ASIO|
|sample rate / 実際のbuffer|48,000 Hz / **512 frames**|
|callback処理時間 平均 / 最大|117.532 us / 857.9 us|
|callback締切超過|0（締切10.667 ms）|
|1秒単位でcallback進行がない区間|0|
|feature / MIDIキュー欠落|0 / 0|
|process CPU time|61.6875秒 / 1800秒、論理1コアの平均3.43%相当|
|最大working set|22,560,768 bytes（約22.6 MB、GUIなし測定プロセス）|
|private memory 開始 / 終了|5,890,048 / 5,730,304 bytes|
|driver報告 input / output latency|728 / 871 samples|
|driver xrunカウンター|**-1（未対応、不明）**|
|物理loopback往復遅延|未測定|

128 framesを要求したがドライバーは512 framesを採用した。したがって、この試験を128 framesの成功とは扱わない。報告input+output latency合計は33.3125 msで、物理測定値ではない。callback処理時間が短いことと、実際の往復遅延が短いことは別。

長時間試験は最後までcallbackが進んだという合格であり、xrun検出機能がないため実音声dropoutが一切ないことを証明しない。最終GUIには1秒間callbackが来ない場合のDEVICE STALLED表示も追加。音声コアはこの30分試験と同じ。試験binaryのSHA256は`test-binary-hashes.txt`、GUIの最終hashは`final-gui-hash.txt`。

## 未測定・未合格

- スタジオの生ドラム録音による一般化、実ドラマーの追従感・音楽性
- UR22Cの物理loopback往復遅延、PAまでの実発音時刻
- 実空間の回り込み、ハウリング、残響、複数マイクの位相差
- 外部MIDI音源の実jitter、VST/SoundFont音質
- バッテリー/熱/他アプリの全負荷条件、USB抜き差しを含む長時間試験

buffer 128/48kHzの片道2.667 msは算術値。実測round-trip latencyとして表示しない。driver xrun=-1は未対応・不明を意味し、0と読み替えない。
