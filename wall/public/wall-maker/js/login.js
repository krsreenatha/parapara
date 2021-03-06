/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var ParaPara = ParaPara || {};

ParaPara.Login = function(sessionName, loggedIn, loggedOut, loginError,
                          loginOptions) {
  this.sessionName        = sessionName;
  this.loggedIn           = loggedIn;
  this.loggedOut          = loggedOut;
  this.loginError         = loginError;
  this.loginOptions       = loginOptions;
  this.watching           = false;
  this.silent             = false;
}

// This tries to re-establish the previous session if there was one and must be
// called first.
ParaPara.Login.prototype.relogin = function() {
  // Persona doesn't call our login/logout method if it agrees about who (if
  // anyone) is logged in, so if we have a valid session we should go ahead and
  // call this.loggedIn/loggedOut and if Persona disagrees it will tell us about
  // it later (after we start watching).
  var alreadyLoggedIn =
      function(response) {
        this.startWatching(response.email);
        this.loggedIn(response.email);
      }.bind(this);
  var notLoggedIn = function() {
        this.startWatching(null);
        this.loggedOut();
      }.bind(this);

  // Don't call loginError if this causes problems
  this.silent = true;

  // See if we still have a valid session
  if (this.haveSessionCookie()) {
    ParaPara.getUrl('/api/whoami',
      // Got a cookie and the server recognises the session
      alreadyLoggedIn,
      // Server error, probably session has expired
      notLoggedIn
    );
  } else {
    notLoggedIn();
  }
}

// This must be called in a trusted context, e.g. a click handler
ParaPara.Login.prototype.login = function() {
  // Check watch() has been called
  if (!this.watching) {
    this.startWatching();
  }
  // Set up options object
  if (!this.loginOptions) {
    this.loginOptions = {};
  }
  this.loginOptions.oncancel = this.onlogout.bind(this);
  // Do report errors since this is an explicit login request (not a relogin)
  this.silent = false;
  // Do login
  navigator.id.request(this.loginOptions);
}

// This must be called in a trusted context, e.g. a click handler
ParaPara.Login.prototype.logout = function() {
  // Check watch() has been called
  if (!this.watching) {
    this.startWatching();
  }
  navigator.id.logout();
}

// ----------------------------
// Internal helpers
// ----------------------------

// Register with Persona for login/logout calls
ParaPara.Login.prototype.startWatching = function(email) {
  if (this.watching)
    return;
  this.watching = true;
  navigator.id.watch({
    loggedInUser: email,
    onlogin: this.gotAssertion.bind(this),
    onlogout: this.onlogout.bind(this)
  });
}

// Clear login state (but DON'T call Persona since this is called in situations
// where Persona already knows we're logged out)
ParaPara.Login.prototype.onlogout = function() {
  this.clearSessionCookie();
  this.loggedOut();
}

// Verify an assertion and if it's ok, finish logging in
ParaPara.Login.prototype.gotAssertion = function(assertion) {
  ParaPara.postUrl('/api/login',
                   { assertion: assertion },
                   // Success, finish logging in
                   function(response) {
                     this.loggedIn(response.email);
                   }.bind(this),
                   // Couldn't verify
                   this.loginFail.bind(this));
}

ParaPara.Login.prototype.loginFail = function(reason, detail) {
  // Known reasons (roughly in order of when they might happen):
  //
  //   send-fail :      something went wrong with sending the request
  //   no-access :      couldn't access the server
  //   timeout :        timed out waiting for response
  //   no-assertion :   didn't send an assertion to verify
  //   server-fail :    something went wrong on our server
  //   browserid-fail : something went wrong with browserid
  //   login-fail :     browser id says status == failure
  //
  var debugMsg = "Login failed [" + reason + "]";
  if (detail) {
    debugMsg += ": " + detail;
  }
  console.debug(debugMsg);

  if (!this.silent && this.loginError) {
    this.loginError(reason, detail);
  }
  this.logout();
}

ParaPara.Login.prototype.clearSessionCookie = function() {
  var expiryDate = new Date();
  expiryDate.setDate(expiryDate.getDate()-1);
  document.cookie =
    this.sessionName + "=; expires=" + expiryDate.toGMTString() + "; path=/";
}

ParaPara.Login.prototype.haveSessionCookie = function() {
  var cookieEq = this.sessionName + "=";
  var cookies = document.cookie.split(';');
  for(var i=0; i < cookies.length; i++) {
    var cookie = cookies[i].trim();
    if (cookie.substring(0, cookieEq.length) == cookieEq)
      return true;
  }
  return false;
}
