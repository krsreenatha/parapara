<!DOCTYPE html>
<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this file,
   - You can obtain one at http://mozilla.org/MPL/2.0/.  -->
<?php
  // We have to make everything absolute so that when the access resources from 
  // e.g. 'server/wall-maker/manage/8' we can still find all the resources
  $wallMakerRoot = dirname($_SERVER['SCRIPT_NAME']);
?>
<html lang="ja">
<head>
  <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
  <title>パラパラアニメーション・壁の作成・管理</title>
  <link rel="stylesheet" href="/css/parapara.css">
  <link rel="stylesheet"
    href="<?php echo $wallMakerRoot; ?>/css/wall-maker.css">
  <link rel="stylesheet"
    href="<?php echo $wallMakerRoot; ?>/css/persona.css">
  <script src="https://login.persona.org/include.js"></script>
  <script>
    var WallMaker = WallMaker || {};
    WallMaker.rootUrl = '<?php echo $wallMakerRoot ?>';
  </script>
  <script src="<?php echo $wallMakerRoot ?>/js/utils.js"></script>
  <script src="<?php echo $wallMakerRoot ?>/js/xhr.js"></script>
  <script src="<?php echo $wallMakerRoot ?>/js/messages.js"></script>
  <script src="<?php echo $wallMakerRoot ?>/js/login.js"></script>
  <script src="<?php echo $wallMakerRoot ?>/js/user-data.js"></script>
  <script src="<?php echo $wallMakerRoot ?>/js/navi.js"></script>
  <script src="<?php echo $wallMakerRoot ?>/js/create-wall.js"></script>
  <script src="<?php echo $wallMakerRoot ?>/js/manage-wall.js"></script>
  <script src="<?php echo $wallMakerRoot ?>/js/login-controller.js"></script>
  <script src="<?php echo $wallMakerRoot ?>/js/qrcode.js" async></script>
</head>
<body>

<div class="top-runner"></div>
<!-- Header -->
<header>
  <div class="header-contents">
    <!-- Login status and Mozilla tab -->
    <nav>
      <div id="loginStatus">
        <span class="login-mail" id="loginMail"></span>
        <span class="logout">(<a href="<?php echo $wallMakerRoot ?>/logout"
          id="logout">サインアウト</a>)</span>
      </div>
      <a href="http://www.mozilla.org" class="mozilla-tab"><img 
        src="/img/tab.png"></a>
    </nav>
    <!-- Title part -->
    <div class="heading">
      <h1>壁メーカー</h1>
    </div>
  </div>
</header>

<div id="page">
  <div class="screen" id="screen-loading">
    <img src="<?php echo $wallMakerRoot?>/img/spinner.gif" class="spinner">
  </div>
  <div class="screen" id="screen-loggedOut" aria-hidden="true">
    <div id="loginError" aria-hidden="true" class="callout">
    </div>
    <div class="callout loginExplain">
      <p>(Mozilla Personaについて説明する)</p>
      <div class="loginButtonLarge">
        <a href="<?php echo $wallMakerRoot ?>/login"
          class="persona-button orange"><span>サインイン</span></a>
      </div>
    </div>
  </div>
  <div class="screen" id="screen-home" aria-hidden="true">
    <div id="wallSummaryHome">
      <nav aria-role="navigation">
        <a href="<?php echo $wallMakerRoot ?>/new"
          class="button newWallLink center">新しい壁を作る</a>
      </nav>
      <hr/>
      <div id="wallSummary" class="thumbnailGrid" aria-hidden="true"></div>
      <div id="wallSummaryLoading">
        <img src="<?php echo $wallMakerRoot ?>/img/spinner.gif" class="spinner">
      </div>
      <div id="wallSummaryError" aria-hidden="true">
        <p>壁の読み込みが失敗しました。</p>
        <button type="button" class="retry"
          onclick="javascript:UserData.updateWalls()">再試行</button>
      </div>
    </div>
    <div id="firstTimeHome" class="callout" aria-hidden="true">
      <p>壁まだありませんので、作りましょう～</p>
      <nav aria-role="navigation">
        <a href="<?php echo $wallMakerRoot ?>/new"
          class="button newWallLink center">新しい壁を作る</a>
      </nav>
    </div>
  </div>
  <div class="screen" id="screen-new" aria-hidden="true">
    <h2>新しい壁を作る</h2>
    <div class="message" id="create-message" aria-hidden="true">
    </div>
    <form name="createWall" action="javascript:CreateWallController.create()">
      <input type="text" name="name" required autocomplete="off"
        maxlength="255" id="create-name"
        placeholder="イベントタイトル">
      <div class="designSelection"></div>
      <div class="center">
        <button type="submit" name="作成" class="submitButton">作成</button>
        <button type="button" class="cancel"
          onclick="javascript:CreateWallController.cancel()"
          >キャンセル</button>
      </div>
    </form>
  </div>
  <div class="screen" id="screen-manage" aria-hidden="true">
    <nav aria-role="navigation">
      <a href="<?php echo $wallMakerRoot ?>/"
        class="button left arrow">戻る</a><br>
    </nav>
    <hr>
    <div id="wall-loading" aria-hidden="false">
      <img src="<?php echo $wallMakerRoot?>/img/spinner.gif" class="spinner">
    </div>
    <div id="wall-info" aria-hidden="true">
      <form name="manageWall">
        <section id="wall-summary">
          <div id="wall-thumbnail"></div>
          <div>
            <div class="withIcon"><input type="text" id="manage-name"
              name="name"></div>
            <ul class="urlList">
              <li>
                <label>壁</label>
                <span class="urlDetails">
                  <span class="highlighted-url">
                    <a id="wallUrl"></a>
                    <span id="wallUrlEdit" aria-hidden="true">
                      <span id="wallUrlBase"></span>
                      <input type="text" id="wallPath">
                    </span>
                  </span>
                  <span id="wallUrlViewControls" class="controls">
                    <button
                      type="button" class="icon editUrl" id="editWallUrl" 
                      title="Edit wall URL">Edit</button>
                  </span>
                  <span id="wallUrlSaveControls" class="controls" 
                    aria-hidden="true">
                    <button type="button" class="small" id="saveWallUrl">
                      保存
                    </button>
                    <button type="button" class="small" id="cancelSaveWallUrl">
                      キャンセル
                    </button>
                  </span>
                </span>
              </li>
              <li>
                <label>エディター</label>
                <span class="urlDetails">
                  <span class="highlighted-url">
                    <a id="editorUrl"></a>
                  </span>
                  <span id="shortEditorUrlBlock">
                    or&nbsp;
                    <span class="highlighted-url">
                      <a id="shortEditorUrl"></a>
                    </span>
                    <button type="button" class="icon qrCode"
                      id="showEditorUrlQrCode"
                      title="Show 2D barcode for this URL">QR</button>
                  </span>
                </span>
              </li>
            </ul>
          </div>
        </section>
        <hr>
        <div class="message" aria-hidden="true">
          <div class="messageText"></div>
        </div>
        <div id="wall-details">
          <menu type="toolbar" aria-role="tablist">
            <a href="#session" aria-role="tab"
              aria-controls="manage-session"
              aria-selected="true">セッション</a><a
              href="#design" aria-role="tab"
              aria-controls="manage-design">デザイン</a><!-- <a
              href="#gallery" aria-role="tab"
              aria-controls="manage-gallery">ギャラリー</a><a
              href="#location" aria-role="tab"
              aria-controls="manage-location">場所</a><a
              href="#access" aria-role="tab"
              aria-controls="manage-access">アクセス</a> --><a
              href="#characters" aria-role="tab"
              aria-controls="manage-characters">キャラクター</a>
          </menu>
          <div class="tab-pages">
            <section id="manage-session" aria-role="tabpanel">
              <div class="wallStatus">
                <span class="currentWallStatus"></span>
                <div class="spinnerContainer">
                  <img src="<?php echo $wallMakerRoot?>/img/spinner.gif" 
                    class="spinner">
                </div>
                <div class="latestSession">
                  <label>最新のセッション</label>
                  <span class="latestSessionTime"></span>
                </div>
              </div>
              <div class="sessionButtons">
                <button type="button"
                  id="manage-startSession">新セッションをスタートする</button>
                <button type="button"
                   id="manage-endSession">セッションを終了する</button>
              </div>
              <div class="callout">
                <p>
                  新しいセッションをスタートすると現在見えるキャラクター全てが見えなくなります。
                </p>
                <p>
                  セッションを終了するとこの壁が新しいキャラクターを受け取りません。
                </p>
              </div>
            </section>
            <section id="manage-design" aria-role="tabpanel" aria-hidden="true">
              <div class="designSelection withIcon"></div>
              <div id="durationControls" class="withIcon">
                <label class="inline">一周期間 (秒）</label>
                <input type="range" min=10 max=1200 value=240 step=10
                  id="duration">
                <span id="duration-units">3h30m</span>
                <button type="button" class="small"
                  id="reset-duration">Reset</button>
              </div>
            </section>
<!--
            <section id="manage-gallery" aria-role="tabpanel"
              aria-hidden="true">
              <dl>
                <dt>ギャラリーに表示</dt>
                <dd class="alongside">
                  <label>
                    <input type="radio" name="manage-galleryDisplay" value="1"
                      checked>On
                  </label>
                  <label>
                    <input type="radio" name="manage-galleryDisplay" value="0">
                    Off
                  </label>
                  <div class="fieldExplain">ギャラリーの説明</div>
                </dd>
                <dt><label class="optional"
                  for="create-eventDescr">イベントの説明</label></dt>
                <dd>
                  <textarea name="eventDescr" class="eventDescr"
                    id="manage-eventDescr"></textarea>
                </dd>
              </dl>
            </section>
            <section id="manage-location" aria-role="tabpanel"
              aria-hidden="true">
              <dl>
                <dt><label class="optional"
                  for="create-eventLocation">場所</label></dt>
                <dd>
                  <input type="text" name="eventLocation" autocomplete="off"
                   maxlength="255" class="eventLocation"
                   id="manage-eventLocation">
              </dl>
            </section>
            <section id="manage-access" aria-role="tabpanel" 
              aria-hidden="true">
              <dl>
                <dt><label class="optional">エディターのパスコード</label></dt>
                <dd>
                  <input type="password" name="passcode" autocomplete="off"
                   maxlength="50" class="passcode" id="manage-passcode">
                  <div class="fieldExplain">パスコードの説明</div>
                </dd>
                <dt><label class="optional">共同制作者</label></dt>
                <dd>
                  <div class="fieldExplain">共同制作者の説明</div>
                </dd>
              </dl>
            </section>
-->
            <section id="manage-characters" aria-role="tabpanel" 
              aria-hidden="true">
              <h3>セッション１</h3>
              <h3>セッション２</h3>
            </section>
          </div>
        </div>
      </form>
    </div>
  </div>
  <div class="screen" id="screen-error" aria-hidden="true">
    <div class="message error">
      <div class="messageText"></div>
      <a href="<?php echo $wallMakerRoot ?>/"
        class="button arrow left center return">戻る</a>
      <button type="button" class="retry"
        onclick="javascript:UserData.updateWalls()">再試行</button>
    </div>
  </div>
</div>
<div class="overlay" aria-hidden="true">
  <div class="container">
    <div class="content" id="qrCode">
      <img>
      <div class="link"></div>
      <button type="button">OK</button>
    </div>
  <div>
</div>
</body>
