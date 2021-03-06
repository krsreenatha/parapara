/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Note that the create wall wizard operates quite differently to the manage
 * wall feature.
 *
 * With the wizard, we create a new history entry each time you go backwards or
 * forwards. Hence, you can use the back/forwards buttons to navigate the
 * wizard. This matches the linear flow of the wizard.
 *
 * However, the URL does not change. It is always wall-maker/new. This is
 * because it doesn't make sense to provide a link to mid-way through a wizard.
 * We remember the current page using session storage so that if the user
 * refreshes during the wizard, we return to the correct page.
 *
 * The manage wall feature on the other hand does not create history entries but
 * DOES update the URL because it makes sense to be able to point to an
 * individual tab in the interface with a URL but you don't want to generate
 * millions of history entries every time you change tab. Restoring the correct
 * tab when you refresh the browser is managed by using document.location.hash.
 */

var CreateWallController =
{
  start: function() {
    this.clear();
  },

  cancel: function() {
    Navigation.goToScreen("./");
    this.clear();
  },

  create: function() {
    // Clear message
    this.messageBox.clear();

    // Show loading screen
    Navigation.showScreen("screen-loading");

    // Send request
    var payload = CreateWallForm.getFormValues();
    ParaPara.postUrl('/api/walls', payload,
                     this.createSuccess.bind(this),
                     this.createError.bind(this));
  },

  createSuccess: function(response) {
    if (typeof response.wallId !== "number") {
      console.debug("Got non-numerical wall id: " + response.wallId);
    }

    // Clear create wall form
    this.clear();

    // Trigger update to wall summary screen
    UserData.updateWalls();

    // Fill in fields of manage wall form using response
    ManageWallController.showNewWall(response);

    // Update the screen
    Navigation.goToScreen("wall/" + response.wallId + "#session");
  },

  createError: function(key, detail) {
    Navigation.showScreen("screen-new");
    this.messageBox.showError(key, detail);
  },

  clear: function() {
    this.messageBox.clear();
    CreateWallForm.clear();
  },

  get messageBox() {
    if (!this._messageBox) {
      this._messageBox = new MessageBox('new');
    }
    return this._messageBox;
  },
};

var CreateWallForm =
{
  _form: null,

  clear: function() {
    this.form.reset();
  },

  getFormValues: function() {
    var result = {};
    result.name = this.form.name.value;
    result.design =
      document.querySelector("#screen-new .designSelection").value;
    return result;
  },

  get form() {
    if (!this._form) {
      this._form = document.forms['createWall'];
    }
    return this._form;
  },
};
