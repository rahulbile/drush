<?php

/**
 * Output formatter 'list'
 *
 * @param $data
 *   The $data parameter is expected to be an array of key / value pairs.
 *   Each key / value pair is passed to some other output formatter for
 *   rendering; the key becomes the label, $metadata['label'], and the
 *   value becomes the $data for the sub-formatter.
 * @param $metadata
 *   'matches' - Specifies the exact kind of list to be rendered in an
 *     array of two elements.  $matches[0] is the full name of the
 *     list format (e.g. 'string-list'), and $matches[1] is the type
 *     of the sub-formatter (e.g. 'string').  If not specified, 'string'
 *     is assumed.
 *
 * Code:
 *
 *   return array('a', 'b', 'c');
 *
 * Output with --format=list: (list of string):
 *
 *   a
 *   b
 *   c
 */
class drush_outputformat_list extends drush_outputformat {
  function validate() {
    // Separate the 'list' and 'filter' metadata from everything
    // else in $engine_config['engine-info']
    $list_metadata = array();
    foreach ($this->engine_config as $key => $value) {
      if ((substr($key, 0, 4) == 'list') || (substr($key, -6) == 'filter') || (substr($key, 0, 5) == 'field') || ($key == 'options')) {
        unset($this->engine_config[$key]);
        $list_metadata[$key] = $value;
      }
    }
    foreach ($this->engine_config['engine-info'] as $key => $value) {
      if ((substr($key, 0, 4) == 'list') || (substr($key, -6) == 'filter')) {
        unset($this->engine_config['engine-info'][$key]);
        $list_metadata[$key] = $value;
      }
    }
    $sub_formatter = isset($list_metadata['list-item-type']) ? $list_metadata['list-item-type'] : 'string';
    $this->sub_engine = drush_load_engine('outputformat', $sub_formatter, NULL, NULL, $this->engine_config);
    if (!is_object($this->sub_engine)) {
      return drush_set_error('DRUSH_INVALID_SUBFORMATTER', dt("The list output formatter could not load its subformatter: !sub", array('!sub' => $sub_formatter)));
    }
    $engine_info = $this->engine_config['engine-info'];
    $this->engine_config = array(
      'engine-info' => array(
        'machine-parsable' => $this->sub_engine->engine_config['engine-info']['machine-parsable'],
      ),
      'metameta' => $this->sub_engine->engine_config,
    ) + $list_metadata;
    return TRUE;
  }

  function format($input, $metadata) {
    $output = '';
    $separator = isset($metadata['list-separator']) ? $metadata['list-separator'] : "\n";
    if (is_array($input)) {
      // If this list is processing output from a command that produces table
      // output, but our subformatter only supports 'single' items (e.g. csv),
      // then we will convert our data such that the output will be the keys
      // of the table rows.
      if (($this->data_type($metadata) == 'format-table') && ($this->supports_single_only($metadata)) && !isset($metadata['list-item'])) {
        // If the user specified exactly one field with --fields, then
        // use it to select the data column to use instead of the array key.
        if (isset($metadata['field-labels']) && (count($metadata['field-labels']) == 1)) {
          $first_label = reset(array_keys($metadata['field-labels']));
          $input = drush_output_get_selected_field($input, $first_label);
        }
        else {
          $input = array_keys($input);
        }
      }
      $first = TRUE;
      $field_selection_control = isset($metadata['list-field-selection-control']) ? $metadata['list-field-selection-control'] : 0;
      $selected_output_fields = false;
      foreach ($input as $label => $data) {
        // If this output formatter is set to print a single item from each
        // element, select that item here.
        if (isset($metadata['list-item'])) {
          $data = isset($data[$metadata['list-item']]) ? $data[$metadata['list-item']] : $metadata['list-item-default-value'];
        }
        // If this formatter supports the --fields option, then filter and
        // order the fields the user wants here.  Note that we need to be
        // careful about when we call drush_select_output_fields(); sometimes,
        // there will be nested formatters of type 'list', and it would not
        // do to select the output fields more than once.
        // 'list-field-selection-control' can be set to a positive number to
        // cause output fields to be selected at a later point in the call chain.
        elseif (is_array($data) && isset($metadata['field-labels'])) {
          if (!$field_selection_control) {
            $data = drush_select_output_fields($data, $metadata['field-labels']);
            $selected_output_fields = true;
          }
        }
        if (empty($metadata['metameta'])) {
          $metameta = $metadata;
          unset($metameta['list-item']);
          unset($metameta['list-item-default-value']);
          $metameta['label'] = $label;
        }
        else {
          $metameta = $metadata['metameta'] + array('label' => $label);
        }
        if ($selected_output_fields) {
          $metameta['list-field-selection-control'] = -1;
        }
        elseif ($field_selection_control) {
          $metameta['list-field-selection-control'] = $field_selection_control - 1;
        }
        foreach (array('field-labels') as $key) {
          if (isset($metadata[$key])) {
            $metameta[$key] = $metadata[$key];
          }
        }
        $formatted_item = $this->sub_engine->process($data, $metameta);
        if ($formatted_item === FALSE) {
          return FALSE;
        }
        if (!$first) {
          $output .= $separator;
        }
        if (($separator != "\n") && (strpos($formatted_item, $separator) !== FALSE)) {
          $formatted_item = drush_wrap_with_quotes($formatted_item);
        }
        $output .= $formatted_item;
        $first = FALSE;
      }
    }
    return $output;
  }
}