# 既知の課題とロードマップ

## 現在の制約

- 検証用プロトタイプ。生ドラム実録音コーパス・ドラマー主観評価・UR22C物理loopbackの合格は未取得。
- 4/4・60〜180 BPM・minorのみ。絶対downbeat、拍子推定、キー認識、楽器分類はない。
- 強い裏拍、均一な16分連打、brush、長い残響、弱音、キックのないパターンは倍半/位相誤りを起こし得る。Tap/Re-syncを用意。
- fillはオンセット密度のheuristic。真のセクション認識ではない。半テンポ/倍テンポへの実際の変更にも数秒追従が遅れる。
- 停止判定は1.25秒以上。短い休符を停止と扱わないトレードオフがある。
- 音響エコーキャンセルなし。PA回り込みで自己追従する恐れ。今回の回り込み試験は合成混合だけで、部屋の検証ではない。
- ベース音色は簡易加算合成。フレーズ生成は確率的ルール、学習済みモデルではない。音楽的自然さは未評価。
- 設定変更は最大2小節遅れる。外部MIDIはOS/worker jitterを含む。内部音源と同時発音するため二重音に注意。
- 全WAVをRAMに読込、最大10分/左channelのみ。GUIに波形・seek・録音機能なし。
- 入出力デバイス設定の永続化、installer、コード署名、自動更新なし。外部MIDI portの追加後はアプリ再起動で再列挙。
- 250 ms以上解析が途切れると休止する。Windowsはhard realtime OSではなく、xrunゼロを保証しない。
- ドライバーがcallback自体を停止するとUIは1秒後にDEVICE STALLEDを表示する。自動でdriverを再初期化せず、Reset Device/再選択で復帰する設計。
- 半/倍テンポに音響的な唯一解がないパターンでは、人のTap入力が必要。

## 次の優先順位

1. **Phase 1スタジオ受け入れ**: 許諾済みの生ドラム録音を60/90/120/150/180、8/16/shuffle、ramp/step/fill、PAあり/なしで収集。人手のbeat注釈からacquisition時間、位相誤差、誤ロック率、停止復帰時間を測定。録音を個人情報なしでfixture化する。
2. **Tracker比較**: aubio、因果的MLモデル、自作DSPを同じ録音で比較。固定されたテスト曲に過適合しないようholdout分離。低信頼時の維持/休止判断をドラマーと調整。
3. **Phase 2音楽性**: 実ベースの奏法、音価の重なり、syncopation、短い問い/応答、motifの記憶、コード進行の選択肢。SoundFont/FluidSynthを別音源backendとして評価。
4. **Phase 3**: セクション状態機械、fill後の応答、guitar/piano、楽器ごとのdensity budgetと音域分離。全楽器が同じHarmonyTimelineを見る。
5. **人間のギター**: chord/key推定とconfidence、HarmonySource優先度（HumanGuitar > Auto）を追加し、低信頼時の保持を設計。
6. **モデル拡張**: 専用推論worker、bounded generation requests、deadline/cancel/epoch、ルールfallback。対象RTX3050でp50/p95/max推論時間とVRAMを実測するまでリアルタイム対応を表示しない。
7. **製品化**: 設定保存、長時間録音ストリーミング、session録音、VST3隔離、installer、配布ライセンス確認、アクセシビリティ/日本語UI。
