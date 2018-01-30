k15lowpow
======================
AMD A シリーズプロセッサ (kaveri アーキテクチャ) 用の省電力制御ソフトです．

Windows 標準の省電力制御では高くなりがちな CPU クロックをより細かく制御します．  
効果につい ては[こちら](https://yoshielise.blogspot.jp/2016/08/k15stat.html)を参照してください．
 
使い方
------
### コンパイル ###
コンパイル・動作には WinRing0 のファイル一式が必要です．
 
### 設定 ###
本ソフトは GUI を持っていません (^^; 設定は [k15tk](http://hbkim.blog.so-net.ne.jp/2015-01-17) を使用し，.cfg ファイルを保存してください．
k15lowpow はその .cfg を使用して config します．  
また，.cfg に以下の行を追加することで，k15lowpow の設定を行います．

	Interval=200
	TargetLoad=75
	FullpowerLoad=95

+   `Interval` :
    CPU 負荷をチェックし，P ステートを変更する時間間隔 [ms] を指定します．
 
+   `TargetLoad` :
    目標とする CPU 使用率 [%] を指定します．この CPU 使用率を超えない範囲で最小クロックとなる P ステートに変更します．  
    例えば TargetLoad=75, 現在のクロック 1000MHz, CPU 使用率 25% だとします．
    この状態・負荷で CPU クロック 333MHz に切り替えれば，CPU 使用率は 75% になるはずです．
    したがって k15lowpow は，333MHz 以上で最もクロックの低い P ステートに切り替えます．

+   `FullpowerLoad` :
    最大クロックに即座に切り替える CPU 使用率 [%] を指定します．この使用率を超えた場合，
    TargetLoad の設定にかかわらず，常に最大クロックとなる P ステートに切り替えます．

### 起動 ###
ショートカットやコマンドラインで，上記 .cfg を指定して起動してください．

	k15lowpow <.cfg file>
