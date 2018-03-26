<?php
/**
 * @file
 * Contains Drush hooks. Inspired by Zen and Omega Tools.
 */

use Drupal\Component\Utility\UrlHelper;

/**
 * Implements hook_drush_command().
 */
function cotailored_base_drush_command() {
  $items = array();

  $items['cotailored_base'] = array(
    'description' => 'Create an Cotailored base subtheme.',
    'arguments' => array(
      'name'         => 'The name of your subtheme.',
    ),
    'options' => array(
      'machine_name' => 'The machine-readable name of your subtheme. This will be auto-generated from the human-readable name if ommited.',
      'description' => 'The description of your subtheme',
      'destination' => 'The destination of your subtheme. Defaults to "all" (sites/all/themes).',
      'kit' => 'The name or url of the starter kit to use. Defaults to "default".',
    ),
    'examples' => array(
      'drush cotailored_base "My Theme"' => 'Creates a Cotailored base subtheme called "My Theme", using the default options.',
      'drush cotailored_base "My Theme" --machine_name=my_theme' => 'Creates a Cotailored base subtheme called "My Theme" with a specific machine name.',
    ),
  );

  return $items;
}

/**
 * Implements hook_drush_help().
 */
function cotailored_base_drush_help($section) {
  switch ($section) {
    case 'drush:cotailored_base':
      return dt('This command will create a Cotailored base subtheme. See examples to get started.');
    break;
  }
}

/**
 * Implements drush_hook_COMMAND().
 */
function drush_cotailored_base($name = NULL, $machine_name = NULL) {

  // If no $name provided, abort.
  if (!$name) {
    drush_print(dt('Theme name missing. See help using drush cotailored_base --help.'));
    return;
  }

  // Determine the theme name.
  if (!isset($name)) {
    $name = drush_get_option('name');
  }

  // Determine the machine name.
  if (!isset($machine_name)) {
    $machine_name = drush_get_option('machine_name');
  }
  if (!$machine_name) {
    $machine_name = $name;
  }
  $machine_name = str_replace(' ', '_', strtolower($machine_name));
  $search = array(
    '/[^a-z0-9_]/', // Remove characters not valid in function names.
    '/^[^a-z]+/',   // Functions must begin with an alpha character.
  );
  $machine_name = preg_replace($search, '', $machine_name);

  // Description of subtheme.
  $description = (drush_get_option('description')) ? trim(drush_get_option('description')) : 'A theme based on Cotailored base.';

  // Determine the path to the new subtheme.
  $subtheme_path = 'themes/custom';
  if ($path = drush_get_option('path')) {
    $subtheme_path = drush_trim_path($path);
  }
  $subtheme_path = drush_normalize_path(drush_get_context('DRUSH_DRUPAL_ROOT') . '/' . $subtheme_path . '/' . $machine_name);

  // Determine the kit to use.
  $kit = (drush_get_option('kit')) ? drush_trim_path(drush_get_option('kit')) : 'cotailored_kickstart';

  // Make a fresh copy of the kit.
  $kit_path = drush_normalize_path(drush_get_context('DRUSH_DRUPAL_ROOT') . '/' . drupal_get_path('theme', 'cotailored_base') . '/kits/' . $kit);

  // Allow kits to be pulled from external urls.
  if (UrlHelper::isValid($kit, TRUE)) {
    $kit_url = $kit;
    $kit_name = 'kit';

    // Get kit name from kit url.
    if (preg_match("/\/cotailored_base\-kit\-([a-z0-9\_]*)\//", $kit_url, $matches)) {
      $kit_name = $kit = $matches[1];
    }

    // Switch to a temp directory.
    $current_dir = getcwd();
    chdir(drush_tempdir());

    if ($filepath = drush_download_file($kit_url)) {
      $filename = basename($filepath);

      // Decompress the zip archive.
      $files = drush_tarball_extract($filename, getcwd(), TRUE);

      // Re-index array.
      // This fixes an issue where a .tag.gz tarball returns a non-zero array.
      $files = array_values($files);
      $kit_path = getcwd() . '/' . $files[0];

      // Set working directory back to the previous working directory.
      chdir($current_dir);
    }
  }

  if (!is_dir(dirname($subtheme_path))) {
    drush_die(dt('The directory "!directory" was not found.', array('!directory' => dirname($subtheme_path))));
  }
  drush_op('drush_copy_dir', $kit_path, $subtheme_path);

  // Alter the contents of the .info file based on the command options.
  $alterations = array(
    'COTAILORED_BASE_SUBTHEME_NAME' => $name,
    'COTAILORED_BASE_SUBTHEME_DESCRIPTION' => $description,
    'COTAILORED_BASE_SUBTHEME_MACHINE_NAME' => $machine_name,
    'hidden: true' => '',
  );

  // Replace all occurrences of '{{machine_name}}' with the machine name of our sub theme.
  $files_to_replace = cotailored_base_get_files_to_make_replacements($kit);
  foreach ($files_to_replace as $file_to_replace) {
    drush_op('cotailored_base_file_str_replace', $subtheme_path . '/' . $file_to_replace, array_keys($alterations), $alterations);
  }

  // Rename files.
  $files_to_rename = array(
    '{{kit}}.info.yml',
    '{{kit}}.libraries.yml',
    '{{kit}}.breakpoints.yml',
    '{{kit}}.theme',
    'config/install/block.block.{{kit}}_branding.yml',
    'config/install/block.block.{{kit}}_content.yml',
    'config/install/block.block.{{kit}}_footer.yml',
    'config/install/block.block.{{kit}}_mainnavigation.yml',
    'config/install/block.block.{{kit}}_messages.yml',
    'config/install/block.block.{{kit}}_page_title.yml',
    'config/install/block.block.{{kit}}_powered.yml',
    'config/install/block.block.{{kit}}_local_tasks.yml',
  );
  foreach ($files_to_rename as $file_to_rename_path) {
    $file_original_path = $subtheme_path . '/' . str_replace('{{kit}}', $kit, $file_to_rename_path);
    $file_new_path = $subtheme_path . '/' . str_replace('{{kit}}', $machine_name, $file_to_rename_path);
    drush_op('rename', drush_normalize_path($file_original_path), drush_normalize_path($file_new_path));
  }

  // Notify user of the newly created theme.
  $message = 'Successfully created the Cotailored base subtheme "!name" created in: !path using the "!kit" kit';

  $message = dt($message . '.', array(
    '!name' => $name,
    '!path' => $subtheme_path,
    '!kit' => $kit,
  ));
  drush_print($message);
}

/**
 * Replace strings in a file.
 */
function cotailored_base_file_str_replace($file_path, $find, $replace) {
  $file_path = drush_normalize_path($file_path);
  $file_contents = file_get_contents($file_path);
  $file_contents = str_replace($find, $replace, $file_contents);
  file_put_contents($file_path, $file_contents);
}

/**
 * Returns an array of files to make string replacements.
 */
function cotailored_base_get_files_to_make_replacements($kit = 'cotailored_kickstart') {
  return array(
    $kit . '.info.yml',
    $kit . '.libraries.yml',
    $kit . '.breakpoints.yml',
    $kit . '.theme',
    'config/install/block.block.' . $kit . '_branding.yml',
    'config/install/block.block.' . $kit . '_content.yml',
    'config/install/block.block.' . $kit . '_footer.yml',
    'config/install/block.block.' . $kit . '_mainnavigation.yml',
    'config/install/block.block.' . $kit . '_messages.yml',
    'config/install/block.block.' . $kit . '_page_title.yml',
    'config/install/block.block.' . $kit . '_powered.yml',
    'config/install/block.block.' . $kit . '_local_tasks.yml',
    'README.md',
  );
}