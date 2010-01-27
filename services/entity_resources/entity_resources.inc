<?php
// $Id: entity_resources.inc,v 1.1.2.1 2009/09/05 13:57:58 marcingy Exp $

/**
 * Implements entity CRUD methods for use as services resources callbacks. Note
 * that the model alters the arguments to prepend the entity type.
 */
class EntityResourceServicesCRUD {
  
  function retrieve($entity_type, $id) {
    $return = entity_load($entity_type, array($id));
    $entity = reset($return);
    // Transform entity into an array of properties to return.
    $wrapper = entity_metadata_wrapper($entity_type, $entity);
    $info = entity_get_info();
    $data = array();
    foreach ($wrapper as $name => $property_info) {
      if (!isset($info[$property_info['type']])) {
        $data[$name] = $wrapper->$name->value();
      }
      // For referenced entities only return the URI.
      elseif (isset($wrapper->$name->url)) {
        $data[$name] = $wrapper->$name->url->value();
      }
    }
    $data['uri'] = services_resource_uri(array($entity_type, $id));
    return $data;
  }
  
  function create($entity_type, array $values = array()) {
    // Make sure all required values are present.
    $info = entity_metadata_get_info($entity_type);
    foreach ($info['properties'] as $key => $property_info) {
      if (!empty($property_info['required']) && !isset($values[$key])) {
        return services_error('Missing property ' . check_plain($key), 406);
      }
    }
    $entity = entity_metadata_entity_create($entity_type, $values);
    entity_metadata_entity_save($entity_type, $entity);
    $entity->uri = services_resource_uri(array($entity_type, $id));
    return $entity;
  }
  
  function update($entity_type, $id, $data) {
    $return = entity_load($entity_type, array($id));
    if ($entity = reset($return)) {
      $wrapper = entity_metadata_wrapper($entity_type, $entity);
      foreach ($data as $name => $value) {
        $wrapper->$name->set($value);
        //TODO: If the property is a entity, get the id out of the value (an uri?)
        // load the referenced entity and pass it.
      }
      return $id;
    }
    return services_error(t('Resource not found'), 404);
  }

  function delete($entity_type, $id) {
    entity_metadata_entity_delete($entity_type, $id);
    return TRUE;
  }
}


/**
 * Provdides a resource feed model for entities.
 */
class EntityResourceFeedModel implements ResourceFeedModel {
  protected $entities, $entityType;
  protected $mode = 'raw';
  protected $itemLength = 'fulltext';

  public function __construct($data, $arguments) {
    $this->entities = $data;
    $this->entityType = $arguments['entity type'];
    if (isset($arguments['mode'])) {
      $this->mode = $arguments['mode'];
    }
    if (isset($arguments['item_length'])) {
      $this->itemLength = $arguments['item_length'];
    }
  }

  public function current() {
    $entity = current($this->entities);
    if ($entity !== FALSE) {
      return new EntityResourceFeedModelItem($entity, $this->entityType, $this->mode, $this->itemLength);
    }
    return FALSE;
  }

  public function key() {
    return key($this->entities);
  }

  public function next() {
    next($this->entities);
  }

  public function rewind() {
    reset($this->entities);
  }

  public function valid() {
    // It's safe to use current as there never should be a boolean
    // in the entity array.
    return current($this->entities) !== FALSE;
  }
  
  /**
   * Prepend the entity type as argument before invoking controller callbacks.
   */
  public static function alterArguments(&$arguments, $model_args) {
    array_unshift($arguments, $model_args['entity type']);
  }
}

class EntityResourceFeedModelItem implements ResourceFeedModelItem {
  protected $entity, $entityType, $entityInfo, $wrapper;
  protected $mode = 'raw';
  protected $itemLength = 'fulltext';
 

  public function __construct($data, $entity_type, $mode = 'raw', $item_length = 'fulltext') {
    $this->entity = $data;
    $this->entityType = $entity_type;
    $this->entityInfo = entity_get_info($entity_type) + array('name property' => 'name');
    $this->wrapper = entity_metadata_wrapper($entity_type, $data);
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
    return $this->wrapper->get($this->entityInfo['name property'])->value();
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
    return url(entity_path($this->entityType, $this->entity), array('absolute' => TRUE));
  }

  /**
   * Gets the created time for the entity
   *
   * @return int
   *  The created time of the entity as a timestamp
   */
  public function getCreated() {
    return isset($this->wrapper->created) ? $this->wrapper->created->value() : 0;
  }

  /**
   * Gets a associative array containing extra properties for the item.
   *
   * @return array
   *  The extra properties of the item as an array
   */
  public function getProperties() {
    $info = entity_get_info();
    $return = array();
    foreach ($this->wrapper as $name => $property_info) {
      if (!isset($info[$property_info['type']])) {
        $return[$name] = $this->wrapper->$name->value();
      }
      // For referenced entities only return the URI.
      elseif (isset($this->wrapper->$name->url)) {
        $return[$name] = $this->wrapper->$name->url->value();
      }
    }
    return $return;
  }
}
