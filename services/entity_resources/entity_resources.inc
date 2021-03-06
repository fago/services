<?php
// $Id: entity_resources.inc,v 1.1.2.1 2009/09/05 13:57:58 marcingy Exp $

module_load_include('runtime.inc', 'services');

/**
 * Implements entity CRUD methods for use as services resources callbacks. Note
 * that the model alters the arguments to prepend the entity type.
 */
class EntityResourceServicesCRUD {
  
  protected static $instances;
  
  /**
   * Gets an instance for the given entity type by making sure there is just
   * one instance per entity type.
   */
  public static function get($entity_type) {
    if (!isset(self::$instances[$entity_type])) {
      return new EntityResourceServicesCRUD($entity_type);
    }
    return self::$instances[$entity_type];
  }
  
  /**
   * Gets a simple PHP array using URI references for some wrapped data.
   *
   * @param $wrapper EntityMetadataWrapper
   *   A wrapper containing the data.
   * @param $resource_path
   *   Pass the path of a service endpoint to have proper resource URIs for that
   *   service generated. Pass FALSE to skip adding resource URIs and keep the
   *   default of NULL to add in resource URIs for the currently active services
   *   endpoint.
   */
  public static function getData($wrapper, $resource_path = NULL) {
    $data = array();
    foreach ($wrapper as $name => $prop) {
      try {
        if ($prop instanceof EntityDrupalWrapper) {
          // For referenced entities just return some metadata and the resource
          // URI, so the caller is able to fetch the entity if desired.
          $data[$name]['entity type'] = $prop->type();
          $data[$name]['entity id'] = $prop->getIdentifier();
          self::addResourceURI($data[$name], $prop, $resource_path);

          if (isset($prop->url)) {
            $data[$name]['url'] = $prop->url->value();
          }
        }
        elseif ($prop instanceof IteratorAggregate) {
          $data[$name] = self::getData($prop);
        }
        else {
          // Decode textual values so the consumer can safely sanitize it.
          $data[$name] = $prop->value(array('decode' => TRUE));
        }
      }
      catch (EntityMetadataWrapperException $e) {
        // A property isn't set - ignore that.
      }
    }
    if ($wrapper instanceof EntityDrupalWrapper) {
      // If there is no property metadata, just return the object properties.
      if (empty($data)) {
        $data = get_object_vars($wrapper->value());
      }
      self::addResourceURI($data, $wrapper, $resource_path);
    }
    return $data;
  }
  

  protected static function addResourceURI(&$data, $wrapper, $resource_path = NULL) {
    if (!isset($resource_path)) {
      $data['resource uri'] = services_resource_uri(array($wrapper->type(), $wrapper->getIdentifier()));
    }
    elseif ($resource_path) {
      $data['resource uri'] = url(implode('/', array($resource_path, $wrapper->type(), $wrapper->getIdentifier())), array('absolute' => TRUE));
    }
  }
  
  
  
  protected $entityType;
  public $callbacks = array();
  
  public function __construct($entity_type) {
    $this->entityType = $entity_type;
    self::$instances[$entity_type] = $this;
  }
  
  public function load($id) {
    $return = entity_load($this->entityType, array($id));
    return reset($return);
  }
  
  /**
   * Retrieve callback.
   */
  public function retrieve($id) {
    $entity = $this->load($id);
    // Transform entity into an array of properties to return.
    $wrapper = entity_metadata_wrapper($this->entityType, $entity);
    return $this->getData($wrapper);
  }
  
  /**
   * Create callback.
   */
  public function create(array $values = array()) {
    // Make sure all required values are present.
    $info = entity_metadata_get_info($this->entity_type);
    foreach ($info['properties'] as $key => $property_info) {
      if (!empty($property_info['required']) && !isset($values[$key])) {
        return services_error('Missing property ' . check_plain($key), 406);
      }
    }
    $wrapper = entity_metadata_entity_create($this->entity_type, $values);
    $wrapper->save();
    // Return the entity in the usual form.
    return $this->getData($wrapper);
  }
  
  /**
   * Update callback.
   */
  public function update($id, $data) {
    if ($entity = $this->load($id)) {
      $wrapper = entity_metadata_wrapper($entity_type, $entity);
      foreach ($data as $name => $value) {
        $wrapper->$name->set($value);
        //TODO: If the property is a entity, get the id out of the value (an uri?)
        // and pass it.
      }
      return $id;
    }
    return services_error(t('Resource not found'), 404);
  }

  /**
   * Delete callback.
   */
  public function delete($id) {
    entity_metadata_entity_delete($entity_type, $id);
    return TRUE;
  }
  
  /**
   * Access callback.
   */
  public function access($op, $args = array()) {
    $entity = ($op != 'create' && !empty($args)) ? $this->load($args[0]) : NULL;
    return $entity !== FALSE && entity_metadata_entity_access($op, $this->entityType, $entity);
  }
  
  /**
   * Magic method: Invokes the defined callback for the method. That way this
   * object may be reused by modules to implement callbacks for actions.
   */
  public function __call($name, $args) {
    if (isset($this->callbacks[$name])) {
      return $this->callbacks[$name]($this, $args, $this->entityType);
    }
  }
}


/**
 * Provdides a resource feed model for entities.
 */
class EntityResourceFeedModel implements ResourceFeedModel {
  protected $items, $entityType;
  protected $mode = 'raw';
  protected $itemLength = 'fulltext';

  public function __construct($data, $arguments) {
    $this->items = $data;
    $this->entityType = $arguments['entity type'];
    if (isset($arguments['mode'])) {
      $this->mode = $arguments['mode'];
    }
    if (isset($arguments['item_length'])) {
      $this->itemLength = $arguments['item_length'];
    }
  }

  public function current() {
    $data = current($this->items);
    if ($data !== FALSE) {
      return new EntityResourceFeedModelItem($data, $this->entityType, $this->mode, $this->itemLength);
    }
    return FALSE;
  }

  public function key() {
    return key($this->items);
  }

  public function next() {
    next($this->items);
  }

  public function rewind() {
    reset($this->items);
  }

  public function valid() {
    // It's safe to use current as there never should be a boolean
    // in the entity array.
    return current($this->items) !== FALSE;
  }

}

class EntityResourceFeedModelItem implements ResourceFeedModelItem {
  protected $data, $entityType, $entityInfo, $wrapper;
  protected $mode = 'raw';
  protected $itemLength = 'fulltext';
 

  public function __construct($data, $entity_type, $mode = 'raw', $item_length = 'fulltext') {
    $this->data = $data;
    $this->entityType = $entity_type;
    $this->entityInfo = entity_get_info($entity_type) + array('name property' => 'name');
    $this->mode = $mode;
    $this->itemLength = $item_length;
  }

  /**
   * Returns the raw entity title
   *
   * @return string
   *  The title of the entity
   */
  public function getName() {
    return $data[$this->entityInfo['name property']];
  }

  public function getDescription() {
    //TODO: As of now there is no metadata about how to view entity.
    // Once the entity metadata module defines that, make use of it.
    return $this->getName();
  }

  /**
   * Returns the absolute url to the entity.
   *
   * @return string
   *  The entity url
   */
  public function getUrl() {
    $id = $data[$this->entityInfo['object keys']['id']];
    $return = entity_load($this->entityType, array($id));
    return url(entity_path($this->entityType, reset($return)), array('absolute' => TRUE));
  }

  /**
   * Gets the created time for the entity
   *
   * @return int
   *  The created time of the entity as a timestamp
   */
  public function getCreated() {
    return isset($data['created']) ? $data['created'] : 0;
  }

  /**
   * Gets a associative array containing extra properties for the item.
   *
   * @return array
   *  The extra properties of the item as an array
   */
  public function getProperties() {
    return $data;
  }
}
