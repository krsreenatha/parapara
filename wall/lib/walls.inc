<?php
/* vim: set syn=php: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

require_once("parapara.inc");
require_once("db.inc");
require_once("exceptions.inc");
require_once("UriUtils.inc");
require_once("designs.inc");
require_once("users.inc");
require_once("utils.inc");

// When updating this, be sure to update login-controller.js as well
define("WALLMAKER_SESSION_NAME", "WMSESSID");

// Given a user id, returns an array of walls owned by that user with the 
// following information:
//
//   wallId
//   event name
//   design thumb URL
//   whether they will display in the gallery or not
//   creation date (UTC)
//   modification date (UTC)
//
// The results will be sorted by creation date such that recently created walls 
// sort before older walls.

function getWallSummaryForUser($email) {
  // Get connection
  $conn =& getDbConnection();

  // Run query
  $res =& $conn->query(
      'SELECT walls.wallId, walls.eventName, designs.designId,'
    . ' walls.galleryDisplay, walls.createDate, walls.modifyDate,'
    . ' walls.designId'
    . ' FROM users, walls'
    . ' LEFT JOIN designs on designs.designId = walls.designId'
    . ' WHERE users.email = '
    . $conn->quote($email, 'text')
    . '   AND walls.owner = users.userId'
    . ' ORDER BY walls.createDate DESC');
  if (PEAR::isError($res)) {
    error_log($res->getMessage() . ', ' . $res->getDebugInfo());
    throw new KeyedException('db-error');
  }

  // Process each array so we can tweak the values and make them a little less 
  // bound to the db schema
  $result = array();
  $conn->setFetchMode(MDB2_FETCHMODE_ASSOC);
  while ($row = $res->fetchRow()) {
    // Get design
    $design = DesignGallery::getById(intval($row['designid']));
    $thumbnail = $design && $design->thumbnail
               ? $design->thumbnail
               : null;

    $result[] =
      array(
        'wallId' => intval($row['wallid']),
        'eventName' => $row['eventname'],
        'thumbnail' => $thumbnail,
        'galleryDisplay' => !!($row['gallerydisplay']),
        'createDate' => $row['createdate'],
        'modifyDate' => $row['modifydate']
      );
  }

  // Tidy up and finish
  $conn->disconnect();

  return $result;
}

// Returns NULL if the wall is not found
function getWallIdFromPath($path) {
  $conn =& getDbConnection();
  $res =& $conn->queryOne(
      'SELECT wallId FROM walls WHERE urlPath = '
      . $conn->quote($path, 'text')
      . ' LIMIT 1',
      'integer');
  if (PEAR::isError($res)) {
    error_log($res->getMessage() . ', ' . $res->getDebugInfo());
    throw new KeyedException('db-error');
  }
  return $res;
}

// Returns NULL if there is no active session for the wall
function getActiveSessionId($wallId) {
  $conn =& getDbConnection();
  $date = gmdate('Y-m-d H:i:s');
  $qdate = $conn->quote($date, 'timestamp');

  $res =& $conn->queryOne(
      'SELECT sessionId FROM sessions'
      . ' WHERE wallId = ' . $conn->quote($wallId, 'integer')
      . ' AND endDate IS NULL'
      . ' ORDER BY sessionId DESC'
      . ' LIMIT 1',
      'integer');
  if (PEAR::isError($res)) {
    error_log($res->getMessage() . ', ' . $res->getDebugInfo());
    throw new KeyedException('db-error');
  }
  return $res;
}

function getWallDuration($wallId) {
  $conn =& getDbConnection();
  $res =& $conn->query(
      'SELECT walls.duration AS duration,'
    . ' designs.duration AS defaultduration'
    . ' FROM walls, designs WHERE'
    . ' walls.designId = designs.designId AND wallId = '
    . $conn->quote($wallId, 'integer')
  );
  if (PEAR::isError($res)) {
    error_log($res->getMessage() . ', ' . $res->getDebugInfo());
    throw new KeyedException('db-error');
  }
  $conn->setFetchMode(MDB2_FETCHMODE_ASSOC);
  $currentDuration = 0;
  if ($row = $res->fetchRow()) {
    $duration = intval($row['duration']);
    $defaultDuration = intval($row['defaultduration']);
    $currentDuration = $duration != 0 ? $duration : $defaultDuration;
  }
  $conn->disconnect();
  return $currentDuration;
}

function getCurrentWallTime($wallId) {
  return getCurrentWallTimeForDuration(getWallDuration($wallId));
}

function getCurrentWallTimeForDuration($duration) {
  $currentTimeMillis = bcmul(microtime(true), 1000);
  return bcmod($currentTimeMillis, $duration);
}

class Wall {
  private $_id      = null;
  private $_design  = null;
  private $metadata = null;
  private $sessions = null;
  private $email    = null;
  private $dirtyFields = array();

  static private $hiddenFields   = array('passcode');
  static private $virtualFields  = array('id', 'passcodeLen',
                                         'wallUrl', 'editorUrl',
                                         'design', 'thumbnail',
                                         'defaultDuration');
  static private $readOnlyFields = array('editorUrlShort');

  public function __construct($id, $email, $metadata) {
    $this->_id      = intval($id);
    $this->email    = trim($email);
    $this->metadata = $metadata;
  }

  public function __get($name) {
    // Check if the field is set (i.e. not hidden, not missing needed parameters
    // etc.)
    if (!isset($this->$name)) {
      return null;
    }

    // Regular metadata
    if (array_key_exists($name, $this->metadata)) {
      return $this->metadata[$name];
    }

    // Virtual fields
    switch ($name) {
      case 'id':
        return $this->_id;

      case 'wallUrl':
        return Wall::getUrlForPath($this->metadata['urlPath']);

      case 'editorUrl':
        return Wall::getEditorUrlForPath($this->metadata['urlPath']);

      case 'passcodeLen':
        return strlen($this->metadata['passcode']);

      case 'design':
        return $this->getDesign();

      case 'thumbnail':
        $design = $this->getDesign();
        return $design ? $design->thumbnail : null;
    }

    return null;
  }

  public function __isset($name) {
    // Hidden fields
    if (in_array($name, self::$hiddenFields))
      return false;

    // Regular metadata
    if (array_key_exists($name, $this->metadata))
      return true;

    // Virtual fields
    switch ($name) {
      case 'id':
        return true;

      case 'wallUrl':
      case 'editorUrl':
        return isset($this->metadata['urlPath']);

      case 'passcodeLen':
        return isset($this->metadata['passcode']);

      case 'design':
        return $this->getDesign() !== null;

      case 'thumbnail':
        $design = $this->getDesign();
        return $design ? $design->thumbnail !== null : false;
    }

    return false;
  }

  public function __set($name, $value) {
    // Check authorisation
    if (!$this->canAdminister())
      throw new KeyedException('no-auth');

    // Check if we recognize the field
    // (XXX We'll have to change this in future to allow setting hidden fields 
    //  even if you can't *get* them)
    if (!isset($this->$name))
      throw new KeyedException('unknown-field');

    // Reject read-only/virtual fields
    // (XXX Might be better to handle each acceptable field below when we 
    //  sanitize and if it's not recognized, throw there)
    if (in_array($name, self::$virtualFields) ||
        in_array($name, self::$readOnlyFields))
      throw new KeyedException('readonly-field');

    // Sanitize values
    switch($name) {
      case 'name':
        $value = wtrim($value);
        // Don't validate the name if it is identical to the current wall name 
        // (which *ought* to be valid) since otherwise we'll get errors about 
        // the name being a duplicate.
        if ($value !== $this->name) {
          Wall::validateWallName($value);
        }
        break;

      case 'urlPath':
        $value = Wall::sanitizePath($value);
        // As with the name, skip the check if the value is identical to the 
        // current
        if ($value !== $this->urlPath) {
          Wall::validatePath($value);
        }
        break;

      case 'designId':
        $value = intval($value);
        if ($value < 1)
          throw new KeyedException('bad-request');
        DesignGallery::validateDesignId($value);
        break;

      case 'duration':
        // Special handling for "null", " NULL" etc.
        // Since we accept the string "500" it makes sense to extend the same 
        // courtesy to the null value.
        if (is_string($value) && strtolower(trim($value)) === "null")
          $value = null;
        if (!is_numeric($value) && !is_null($value))
          throw new KeyedException('bad-request');
        $value = toIntOrNull($value);
        if ($value < 1 && $value !== null)
          throw new KeyedException('bad-request');
        break;
    }

    // Check if the value is actually different
    // (XXX Likewise we'll need special handling for hidden fields here)
    if ($this->$name === $value)
      return;

    // Regular metadata
    if (array_key_exists($name, $this->metadata)) {
      $this->metadata[$name] = $value;
      // Mark field as dirty
      if (!in_array($name, $this->dirtyFields))
        array_push($this->dirtyFields, $name);
    }

    // Special handling for fields that have calculated values
    switch ($name) {
      case 'urlPath':
        $editorUrlShort = Wall::getShortenedEditorUrlForPath($value);
        if ($editorUrlShort &&
            $editorUrlShort !== @$this->metadata['editorShortUrl']) {
          $this->metadata['editorUrlShort'] = $editorUrlShort;
          if (!in_array('editorUrlShort', $this->dirtyFields))
            array_push($this->dirtyFields, 'editorUrlShort');
        }
        break;

      case 'designId':
        // Reset cached design
        $this->_design = null;
        break;
    }
  }

  public function save() {
    // Check authorisation
    if (!$this->canAdminister()) {
      throw new KeyedException('no-auth');
    }

    // Check there is something to save
    if (count($this->dirtyFields) === 0)
      return array();

    // Start connection
    $conn =& getDbConnection();

    // Build up pieces of query and result array
    $changedFields = array();
    $assignments = array();
    foreach ($this->dirtyFields as $name) {
      // Determine the field name
      $mappedFields = array('name' => 'eventName',
                            'editorUrlShort' => 'editorShortUrl');
      $fieldName = array_key_exists($name, $mappedFields)
                 ? $mappedFields[$name]
                 : $name;

      // Determine the type of the field
      switch ($fieldName) {
        case 'designId':
        case 'duration':
          $type = 'integer';
          break;

        case 'galleryDisplay':
          $type = 'boolean';
          break;

        case 'createDate':
        case 'modifyDate':
          $type = 'timestamp';
          break;

        default:
          $type = 'text';
          break;
      }
      array_push($assignments,
        $conn->quoteIdentifier($fieldName) . ' = '
        . $conn->quote($this->$name, $type));

      // Assign changed fields (including virtual fields where the dependent 
      // field has changed)
      // (XXX Make sure we don't return hidden fields)
      switch ($name) {
        case 'urlPath':
          $changedFields['wallUrl'] = $this->wallUrl;
          $changedFields['editorUrl'] = $this->editorUrl;
          break;

        case 'designId':
          $changedFields['designId'] = $this->designId;
          $changedFields['thumbnail'] = $this->design->thumbnail;
          $changedFields['defaultDuration'] = $this->design->duration;
          break;

        case 'duration':
          $changedFields['duration'] = $this->duration;
          $changedFields['defaultDuration'] = $this->defaultDuration;

        default:
          $changedFields[$name] = $this->$name;
          break;
      }
    }

    // Update modified date
    array_push($assignments,
      'modifyDate = ' . $conn->quote(gmdate('Y-m-d H:i:s'), 'timestamp'));

    // Run query
    $query = 'UPDATE walls SET '
      . implode(', ', $assignments)
      . ' WHERE wallId = ' . $conn->quote($this->id, 'integer');
    $res =& $conn->exec($query);
    if (PEAR::isError($res)) {
      error_log($res->getMessage() . ', ' . $res->getDebugInfo());
      throw new KeyedException('db-error');
    }
    $conn->disconnect();

    // Reset dirty fields
    $this->dirtyFields = array();

    return $changedFields;
  }

  public function asArray() {
    // Fields to export if available
    //
    // Associative keys map the regular value name used by users of this class 
    // to another more export-friendly value.
    $keys = array('id' => 'wallId', 'name',
                  'eventLocation',
                  'eventDescr',
                  'wallUrl', 'editorUrl', 'editorUrlShort',
                  'duration', 'defaultDuration',
                  'ownerEmail', 'passcodeLen',
                  'designId',
                  'thumbnail',
                  'status', 'latestSession');

    $result = array();
    foreach ($keys as $localKey => $keyToExport) {
      $localKey = is_int($localKey) ? $keyToExport : $localKey;
      if (isset($this->$localKey)) {
        $result[$keyToExport] = $this->$localKey;
      }
    }
    return $result;
  }

  public function startSession($currentSessionId, $datetime) {
    // Sanitize input
    $currentSessionId = toIntOrNull($currentSessionId);

    // Check authorisation
    if (!$this->canAdminister()) {
      throw new KeyedException('no-auth');
    }

    // Work out the latest session and if it needs closing
    $latestSession = $this->latestSession;

    // Is the passed-in session ID correct?
    if (toIntOrNull($latestSession['id']) !== $currentSessionId) {
      return false;
    }

    // Close the latest session if it's open
    if (@$latestSession['endDate'] === null) {
      $this->endSession($currentSessionId, $datetime);
    }

    // Get sessionId for next session
    $conn =& getDbConnection();
    $quotedWallId = $conn->quote($this->id, 'integer');
    $newSessionId =& $conn->queryOne(
        'SELECT IFNULL(MAX(sessionId)+1, 1)'
        . ' FROM sessions WHERE wallId = ' . $quotedWallId
        . ' LIMIT 1',
        'integer');
    if (PEAR::isError($newSessionId)) {
      error_log($newSessionId->getMessage() . ', '
                . $newSessionId->getDebugInfo());
      throw new KeyedException('db-error');
    }

    // Start a new session
    $query = 'INSERT INTO sessions(wallId, sessionId, beginDate)'
      . ' VALUES(' . $quotedWallId
      . ',' . $conn->quote($newSessionId, 'integer')
      . ',' . $conn->quote($datetime, 'timestamp')
      . ')';
    $res =& $conn->exec($query);
    if (PEAR::isError($res)) {
      error_log($res->getMessage() . ', ' . $res->getDebugInfo());
      throw new KeyedException('db-error');
    }

    // Update latest session
    $this->metadata['latestSession']['id']    = $newSessionId;
    $this->metadata['latestSession']['start'] = $datetime;
    $this->metadata['latestSession']['end']   = null;
    $this->metadata['status'] = 'running';

    $conn->disconnect();

    return true;
  }

  public function endSession($currentSessionId, $datetime) {
    // Sanitize input
    $currentSessionId = toIntOrNull($currentSessionId);

    // Check authorisation
    if (!$this->canAdminister()) {
      throw new KeyedException('no-auth');
    }

    // Check that:
    // (a) there is actually a session to close
    // (b) the session is not already closed
    // (c) the passed-in session ID matches the latest session
    $latestSession = $this->latestSession;
    if ($latestSession === null ||
        $latestSession['end'] !== null ||
        $latestSession['id'] !== $currentSessionId)
      return false;

    // End session
    $conn =& getDbConnection();
    $query = 'UPDATE sessions'
      . ' SET endDate=' . $conn->quote($datetime, 'timestamp')
      . ' WHERE sessionId=' . $conn->quote($currentSessionId, 'integer')
      . ' AND wallId=' . $conn->quote($this->id, 'integer');
    // XXX This is wrong
    $res =& $conn->exec($query);
    if (PEAR::isError($res)) {
      error_log($res->getMessage() . ', ' . $res->getDebugInfo());
      throw new KeyedException('db-error');
    }
    $conn->disconnect();

    // Update latest session
    $this->metadata['latestSession']['end'] = $datetime;
    $this->metadata['status'] = 'finished';

    // Return true since we made a change
    return true;
  }

  public static function getPathForName($name) {
    // Sanitize
    $path = Wall::sanitizePath($name);

    // Check for duplicates
    // If we have a clash, just generate a random ID and use that--the user can 
    // make it something more sensible later if they wish
    if (!Walls::isPathUnique($path))
      $path = rawurlencode(uniqid());

    return $path;
  }

  public static function getUrlForPath($path) {
    return getCurrentServer() . '/wall/' . rawurlencode($path);
  }

  public static function getEditorUrlForPath($path) {
    global $config;
    return $config['editor']['url'] . rawurlencode($path);
  }

  // Returns the shortened editor URL for a given path or NULL if no shortening 
  // service is available
  public static function getShortenedEditorUrlForPath($path) {
    $editorUrl = Wall::getEditorUrlForPath($path);

    $editorUrlShort = shortenUrl($editorUrl);
    return $editorUrlShort == $editorUrl ? NULL : $editorUrlShort;
  }

  public static function validateWallName($name) {
    // Check name has been trimmed
    if ($name !== wtrim($name))
      throw new KeyedException('bad-name');

    // Check name is not empty
    if (!strlen($name))
      throw new KeyedException('empty-name');

    // Check name is unique
    if (!Walls::isNameUnique($name)) {
      error_log("Wall with name '$name' already exists.");
      throw new KeyedException('duplicate-name');
    }
  }

  public static function sanitizePath($path) {
    $path = mb_convert_kana(wtrim($path), "as", "utf-8");

    // Make lowercase, and
    // convert whitespace and URL-like characters to -
    $path = strtr(strtolower($path), " \t/:.", '-----');

    return $path;
  }

  public static function validatePath($path) {
    // Check the path has been sanitized
    if ($path !== self::sanitizePath($path))
      throw new KeyedException('bad-path');

    // Check the path exists
    if (!strlen($path))
      throw new KeyedException('bad-path');

    // Check it is unique
    if (!Walls::isPathUnique($path)) {
      error_log("Wall with path '$path' already exists.");
      throw new KeyedException('duplicate-path');
    }
  }

  public function canAdminister() {
    // In the future we will check if the wall has been shared with the current 
    // user or not
    return $this->isOwner();
  }

  public function isOwner() {
    if (!isset($this->email) || $this->ownerEmail === null)
      return false;
    return $this->email == $this->ownerEmail;
  }

  private function getDesign() {
    if ($this->_design === null && $this->designId) {
      $this->_design = DesignGallery::getById($this->designId);
    }
    return $this->_design;
  }
}

class Walls {
  public static function create($name, $designId, $email) {
    // Get connection
    $conn =& getDbConnection();

    // Validate owner email
    $email = trim($email);
    if (!strlen($email) || !filter_var($email, FILTER_VALIDATE_EMAIL)) {
      error_log("Bad or missing email when creating wall");
      throw new KeyedException('bad-email', 'Bad email for user');
    }

    // Fetch user ID for owner
    $ownerId = Users::getOrCreateFromEmail($email);

    // Validate wall name
    $name = wtrim($name);
    Wall::validateWallName($name);

    // Validate design ID
    $design = DesignGallery::getById($designId);
    if (!$design) {
      error_log("Could't find design ID " . $designId);
      throw new KeyedException('design-not-found', 'ID not found');
    }

    // Get URLs
    $path      = Wall::getPathForName($name);
    $wallUrl   = Wall::getUrlForPath($path);
    $editorUrl = Wall::getEditorUrlForPath($path);

    // Create shortened versions
    $editorShortUrl = Wall::getShortenedEditorUrlForPath($path);

    // Store in database
    $date = gmdate('Y-m-d H:i:s');
    $query =
      'INSERT INTO walls'
      . ' (owner, designId, eventName, galleryDisplay, urlPath,'
      . '  editorShortUrl, createDate, modifyDate)'
      . ' VALUES '
      . '(' . $conn->quote($ownerId, 'integer')
      . ',' . $conn->quote($designId, 'integer')
      . ',' . $conn->quote($name, 'text')
      . ',TRUE' // galleryDisplay
      . ',' . $conn->quote($path, 'text')
      . ',' . (($editorShortUrl) ? $conn->quote($editorShortUrl, 'text')
                                 : 'NULL')
      . ',' . $conn->quote($date, 'timestamp')
      . ',' . $conn->quote($date, 'timestamp')
      . ')';
    $res =& $conn->query($query);
    if (PEAR::isError($res)) {
      error_log($res->getMessage() . ', ' . $res->getDebugInfo());
      throw new KeyedException('db-error');
    }

    // Get ID of newly created wall
    $wallId = $conn->lastInsertID('walls', 'wallId');
    if (PEAR::isError($wallId)) {
      error_log($res->getMessage() . ', ' . $res->getDebugInfo());
      throw new KeyedException('db-error');
    }

    // Return newly created wall object
    // (We could just create the Wall object from the information we have on 
    // hand but it's just simpler to re-use the getById look up code and saves 
    // us from bugs where we set things in one place and not the other)
    return self::getById($wallId, $email);
  }

  public static function getById($id, $email) {
    $conn =& getDbConnection();
    $row =& $conn->queryRow(
        'SELECT walls.designId'
      . ' ,eventName, eventDescr, eventLocation, eventType, eventFinish'
      . ' ,urlPath, editorShortUrl'
      . ' ,walls.duration, galleryDisplay, passcode'
      . ' ,designs.duration AS defaultDuration'
      . ' ,users.email AS ownerEmail'
      . ' ,sessionId, beginDate as sessionStart, endDate as sessionEnd'
      . ' FROM walls'
      . ' INNER JOIN designs ON walls.designId = designs.designId'
      . ' LEFT JOIN users ON'
      . '   walls.owner = users.userId'
      . ' LEFT JOIN sessions ON'
      . '   sessions.sessionSerial = '
      . '   (SELECT MAX(sessionSerial) FROM sessions'
      . '    WHERE sessions.wallId = ' . $conn->quote($id, 'integer') . ')'
      . ' WHERE walls.wallId = ' . $conn->quote($id, 'integer')
      . ' LIMIT 1',
      null,
      MDB2_FETCHMODE_ASSOC
    );
    $conn->disconnect();
    if (PEAR::isError($row)) {
      error_log($row->getMessage() . ', ' . $row->getDebugInfo());
      throw new KeyedException('db-error');
    }

    // Check if wall was found
    if ($row === null)
      return null;

    // Check auth
    $hasAccess = $email == $row['owneremail'];

    // Public information
    $metadata["name"]           = $row['eventname'];
    $metadata["designId"]       = toIntOrNull($row['designid']);
    $metadata["urlPath"]        = $row['urlpath'];
    $metadata["editorUrlShort"] = $row['editorshorturl'];

    // Privileged information
    if ($hasAccess) {
      $metadata["ownerEmail"] = $row['owneremail'];
    }

    // Session information
    if ($row['sessionid']) {
      if ($hasAccess) {
        $session['id'] = intval($row['sessionid']);
        $session['start'] = $row['sessionstart'];
        $session['end'] = $row['sessionend'];
        $metadata['latestSession'] = $session;
      }
      $metadata['status'] = $row['sessionend'] ? 'finished' : 'running';
    } else {
      if ($hasAccess) {
        $metadata['latestSession'] = null;
      }
      $metadata['status'] = 'finished';
    }

    // Duration information
    $metadata["duration"] = toIntOrNull($row['duration']);
    $metadata["defaultDuration"] = toIntOrNull($row['defaultduration']);

    return new Wall($id, $email, $metadata);
  }

  public static function isNameUnique($name) {
    return !self::recordExists('walls', 'eventName', $name, 'text');
  }

  public static function isPathUnique($path) {
    return !self::recordExists('walls', 'urlPath', $path, 'text');
  }

  private static function recordExists($table, $field, $value, $type) {
    $conn =& getDbConnection();
    $res =& $conn->query(
        "SELECT * FROM $table WHERE BINARY $field = "
        . $conn->quote($value, $type)
        . " LIMIT 1");
    if (PEAR::isError($res)) {
      error_log($res->getMessage() . ', ' . $res->getDebugInfo());
      throw new KeyedException('db-error');
    }
    $exists = $res->numRows() > 0;
    $res->free();
    return $exists;
  }
}

?>
