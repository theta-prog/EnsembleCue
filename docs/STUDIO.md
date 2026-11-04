# Windows / UR22C スタジオ接続

1. UR22CをPCにUSB接続。Yamaha Steinberg USB Driverをメーカー公式から導入済みか確認。既存ドライバーをこの作業で更新・置換していない。
2. ドラムマイクをUR22C INPUT 1へ。最初はキックとスネアが十分入る近い位置で試す。コンデンサーマイクの+48Vは、そのマイクの説明書に従う。
3. UR22C LINE OUTPUT L/RをミキサーのLINE入力へ。最初はPA音量を下げる。マイクの生音は本アプリから出力しない。インターフェース側のダイレクトモニターで別経路の回り込みがないか確認する。
4. アプリのAudio device typeでASIOを選び、Yamaha Steinberg USB ASIOを選択。入力1/2、出力1/2を設定。Input 2を使う場合はデバイス画面でも入力2を有効化する。
5. まず48 kHz / 128 framesを候補に試す。48 kHzでは64/128/256 framesが片道バッファ時間約1.33/2.67/5.33 ms。これはADC/DAC・USB・driver safety bufferを含まない計算値。dropoutが出れば256へ上げる。デバイスが認める設定だけがGUIに表示される。
6. Live microphoneを選択。停止中でも入力メーターが動く。強打でclipしないようマイクゲイン調整。音が小さい場合はOnset gateを下げるが、PAや残響を拾いやすくなる。
7. Following clickだけをONにしてStart。均等なキック/スネアを数拍叩き、FOLLOWINGを確認する。誤った倍半テンポならTap tempoを3〜5回。その後も自動追従する。Re-syncは履歴を破棄し取り直す。
8. AI bassistをON。Rock/Funk/Shuffle、Autoまたは手動キー、密度/変化量/音量を設定。最初は内蔵音源で検証し、外部MIDI音源は次に試す。
9. 演奏停止後に伴奏が休むこと、再開で復帰することを確認。自動停止は短い休符を許容するため即時ではない。確実に止める操作はStop。
10. Save metricsでドライバー報告遅延、callback時間、deadline超過、xrun、queue dropをCSV保存する。

UR22Cの[公式マニュアル](https://download.steinberg.net/downloads_hardware/UR-C/Manuals/UR22C_Operation_Manual_English.pdf)はYamaha Steinberg USB DriverのASIOタブでbuffer sizeを設定する手順を示している。アプリから要求した値がそのまま採用されない場合、アプリのControl Panelから確認する。別のASIO/WASAPIアプリを同時に開いたまま設定を変更するとストリームが停止する場合がある。音声試験は単独で行う。

## 回り込み

本MVPは音響エコーキャンセルや音源分離を実装しない。PAのベースやクリックがマイクに強く入ると、自分の伴奏へロックしてドラマーを追わなくなる可能性がある。まずヘッドフォンで検証し、次にマイクの背面をPAへ向ける、距離・指向性・ゲインを調整する。アプリが入力モニターを出さなくても、部屋の音響による回り込みは残る。

## 発音時刻の校正

現在はドライバー報告input/output latencyを加算した拍先読み。Timing trimの正値でさらに先行、負値で遅らせる。正しい値は物理ループバックで測る。

測定時は低いline levelで、output→空きline inputへ接続し、入力クリックと出力クリックを別トラックに記録する。マイク入力へ不用意に大音量のline信号を入れない。基準拍注釈との差の中央値、p95、jitterを算出する。ループバック配線と生ドラムでの測定は今回未実施。

## 録音で再現

Output volumeは内蔵音源・デモ/WAVモニターの音量です。外部MIDI音源の音量は外部音源側で調整してください。内蔵音源とMIDIは同時に動くので、外部音源だけを使う場合はOutput volumeを0にします。

Load drum WAVでドラムだけの録音を選ぶ。最大10分、左channelを解析。再生sample rateが異なる場合は簡易線形resample。Startで先頭から開始、最後は無音となりtrackerが停止する。音声処理スレッドでディスクアクセスはしない。実マイクを同時解析しない。

実録音のCLI検証:

```powershell
.\build\drum_render_artefacts\Release\drum_render.exe "C:\audio\drums.wav" "$PWD\results\session.wav"
```

`session.wav`はベース/クリックのみ、`session.mid`は音符、`session.csv`は解析履歴。音源をクラウドへ送信しない。
