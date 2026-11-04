# v0.2.0 — 3楽器の合奏

2026-11-04

## 操作

AI bassist / AI guitar / AI pianoで参加する楽器を選択。Bass/Guitar/Piano levelは内蔵音源の音量、Output volumeはクリックとモニターを含む出力全体の音量。既存MIDI出力はchannel 1がベース、2がギター、3がピアノ。外部音源の音色と音量は外部機器側で設定する。

SectionでAuto / Groove / Build / Breakdownを選択。Autoはドラムの密度とfill候補に応じて決定する。Grooveはギターの刻みとピアノのコード、Buildはギターの刻みを増やしピアノが短いアルペジオで応答、Breakdownはギターを休ませピアノの長いコードにする。設定反映は最大2小節先。fill候補が検出された間、ライブ再生ではギター・ピアノをreleaseし、MIDIの発音中の音も止める。

## 構造

Arrangerがベース生成器から得たコードルート・3度・5度を全楽器へ共有する。マイナースケール内のダイアトニックな和音を使用。ギターは低〜中音域のパワーコードを少しずつずらして発音し、ピアノは上の音域で3度を含むコードや応答を担当する。Rock/Funk/Shuffleでギターの発音位置が変わる。ベースのモチーフ変化と共有進行は継続する。

Arrangementは3つのPhraseをまとめた固定容量データ。各楽器のSchedulerが共通のBeatClockを参照する。ベースは単音、ギター・ピアノは各8声の固定ポリフォニー。各楽器の単純な加算合成と減衰包絡を実装。音色の写実性は次の改善項目。

MIDIのnote offは最大32音の固定配列で拍座標を管理。同じchannel/pitchの再発音前に古い音を止め、楽器OFF、停止、epoch変更でもreleaseする。MIDIキュー欠落時はワーカーで3チャンネルにAll Notes Offを送る。デバイス呼出しは音声callback外で行う。

## 確認結果

- Releaseビルド成功。CTestは既存コア・sample rate・新規ensembleの3/3合格。`results/ensemble-ctest.txt`。
- 12キー×16小節で共有和声、音符範囲、容量を確認。Breakdown/fill時の役割、独立ミュート、各音の一回だけの発音、古いepoch拒否、release後の無音、有限の合成出力を確認。
- 30秒の合奏WAVはpeak 0.2802、RMS 0.08336。録音WAVの再入力による最大差は1.192e-7。MIDIはNote On/Off各192、channel 1/2/3の発音数76/83/33。`results/v02/render-check.json`。
- UR22C ASIO・48 kHz・実buffer 512 framesで3楽器同時の60秒測定が完走。callback平均320.463 us、最大1,157.3 us、締切超過0、callback停止0、feature/MIDIキュー欠落0。CPUは論理1コアの平均4.95%相当、最大working set約21.6 MB。xrunカウンターは非対応、物理往復遅延は今後の測定対象。`results/ensemble-audio-device-metrics.csv`、`ensemble-audio-statistics.json`。
- 測定の初期化には時間がかかったが、終了ログとCSVの0〜60秒を確認。GUIで3楽器のON/OFF・音量・Section選択の表示とレイアウトも確認。v0.1の30分記録は履歴として維持する。

## 次の機能

設定の保存、コード進行の手動編集、セッション録音、より自然な音源、低信頼度時の伴奏制御、fill後の展開認識、生成モデルの差し替えを順に検討する。現在のAutoセクションは密度に基づくヒューリスティックであり、曲構造の認識は今後の工程。
