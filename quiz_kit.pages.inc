<?php
/**
 * @file
 * User-facing pages: quiz listing, take quiz, results.
 */

/**
 * Page callback: list all published quizzes.
 */
function quiz_kit_list_page() {
  $nodes = db_query(
    "SELECT n.nid, n.title FROM {node} n WHERE n.type = 'quiz_kit_quiz' AND n.status = 1 ORDER BY n.title"
  )->fetchAll();

  if (empty($nodes)) {
    return t('No quizzes are available.');
  }

  $items = array();
  foreach ($nodes as $n) {
    $items[] = l(check_plain($n->title), 'node/' . $n->nid);
  }

  return theme('item_list', array('items' => $items));
}

// ---------------------------------------------------------------------------
// Take quiz
// ---------------------------------------------------------------------------

/**
 * Page callback: take a quiz.
 */
function quiz_kit_take_page($node) {
  global $user;

  // Check attempt limits.
  $max = (int) _quiz_kit_node_setting($node, 'max_attempts', 0);
  if ($max > 0) {
    $attempts = db_query(
      'SELECT COUNT(*) FROM {quiz_kit_result} WHERE nid = :nid AND uid = :uid AND finished > 0',
      array(':nid' => $node->nid, ':uid' => $user->uid)
    )->fetchField();
    if ($attempts >= $max) {
      return t('You have reached the maximum number of attempts for this quiz.');
    }
  }

  // Load questions.
  $questions = _quiz_kit_load_questions($node->nid);
  if (empty($questions)) {
    return t('This quiz has no questions yet.');
  }

  return backdrop_get_form('quiz_kit_take_form', $node, $questions);
}

/**
 * Form: quiz-taking form.
 */
function quiz_kit_take_form($form, &$form_state, $node, $questions) {
  $form_state['quiz_node'] = $node;
  $form_state['questions'] = $questions;

  // Start a result record if not already in progress.
  if (empty($form_state['result_id'])) {
    global $user;
    $form_state['result_id'] = db_insert('quiz_kit_result')
      ->fields(array(
        'nid' => $node->nid,
        'uid' => $user->uid,
        'score' => 0,
        'passed' => 0,
        'started' => REQUEST_TIME,
        'finished' => 0,
      ))
      ->execute();
  }

  $form['#tree'] = TRUE;

  foreach ($questions as $q) {
    $answers = _quiz_kit_load_answers($q->vid);
    $form['questions'][$q->id] = _quiz_kit_question_element($q, $answers);
  }

  $form['actions']['#type'] = 'actions';
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Submit quiz'),
  );

  return $form;
}

/**
 * Submit: quiz-taking form — grade and save results.
 */
function quiz_kit_take_form_submit($form, &$form_state) {
  $node = $form_state['quiz_node'];
  $questions = $form_state['questions'];
  $result_id = $form_state['result_id'];
  $responses = $form_state['values']['questions'];

  $total = count($questions);
  $correct = 0;

  foreach ($questions as $q) {
    $given = isset($responses[$q->id]) ? $responses[$q->id] : NULL;
    $is_correct = _quiz_kit_grade_answer($q, $given);
    if ($is_correct) {
      $correct++;
    }

    db_insert('quiz_kit_result_answer')
      ->fields(array(
        'result_id' => $result_id,
        'question_id' => $q->id,
        'question_vid' => $q->vid,
        'answer_given' => is_array($given) ? serialize($given) : (string) $given,
        'is_correct' => (int) $is_correct,
      ))
      ->execute();
  }

  $score = $total > 0 ? round(($correct / $total) * 100, 1) : 0;
  $pass_rate = (int) _quiz_kit_node_setting($node, 'pass_rate', 75);
  $passed = $score >= $pass_rate ? 1 : 0;

  db_update('quiz_kit_result')
    ->fields(array(
      'score' => $score,
      'passed' => $passed,
      'finished' => REQUEST_TIME,
    ))
    ->condition('id', $result_id)
    ->execute();

  $form_state['redirect'] = 'node/' . $node->nid . '/results/' . $result_id;
}

// ---------------------------------------------------------------------------
// Results pages
// ---------------------------------------------------------------------------

/**
 * Page callback: list all results for a quiz (admin/author view).
 */
function quiz_kit_results_list_page($node) {
  $results = db_query(
    'SELECT r.*, u.name FROM {quiz_kit_result} r
     LEFT JOIN {users} u ON u.uid = r.uid
     WHERE r.nid = :nid AND r.finished > 0
     ORDER BY r.finished DESC',
    array(':nid' => $node->nid)
  )->fetchAll();

  if (empty($results)) {
    return t('No completed attempts yet.');
  }

  $header = array(t('User'), t('Score'), t('Passed'), t('Date'), t('Detail'));
  $rows = array();
  foreach ($results as $r) {
    $rows[] = array(
      check_plain($r->name ?: t('Anonymous')),
      $r->score . '%',
      $r->passed ? t('Yes') : t('No'),
      format_date($r->finished, 'short'),
      l(t('View'), 'node/' . $node->nid . '/results/' . $r->id),
    );
  }

  return theme('table', array('header' => $header, 'rows' => $rows));
}

/**
 * Page callback: single result detail.
 */
function quiz_kit_result_detail_page($node, $result) {
  $answers = db_query(
    'SELECT ra.*, r.question_text FROM {quiz_kit_result_answer} ra
     JOIN {quiz_kit_question_revision} r ON r.vid = ra.question_vid
     WHERE ra.result_id = :rid',
    array(':rid' => $result->id)
  )->fetchAll();

  $show_correct = (bool) _quiz_kit_node_setting($node, 'show_answers', TRUE);

  return theme('quiz_kit_results', array(
    'node' => $node,
    'result' => $result,
    'answers' => $answers,
    'show_correct' => $show_correct,
  ));
}

/**
 * Page callback: a user's own result history.
 */
function quiz_kit_user_results_page($account) {
  $results = db_query(
    "SELECT r.*, n.title FROM {quiz_kit_result} r
     JOIN {node} n ON n.nid = r.nid
     WHERE r.uid = :uid AND r.finished > 0
     ORDER BY r.finished DESC",
    array(':uid' => $account->uid)
  )->fetchAll();

  if (empty($results)) {
    return t('No quiz results yet.');
  }

  $header = array(t('Quiz'), t('Score'), t('Passed'), t('Date'), t('Detail'));
  $rows = array();
  foreach ($results as $r) {
    $rows[] = array(
      l(check_plain($r->title), 'node/' . $r->nid),
      $r->score . '%',
      $r->passed ? t('Yes') : t('No'),
      format_date($r->finished, 'short'),
      l(t('View'), 'node/' . $r->nid . '/results/' . $r->id),
    );
  }

  return theme('table', array('header' => $header, 'rows' => $rows));
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Loads questions for a quiz node, in weight/id order.
 */
function _quiz_kit_load_questions($nid) {
  return db_query(
    'SELECT q.* FROM {quiz_kit_question} q WHERE q.nid = :nid ORDER BY q.weight, q.id',
    array(':nid' => $nid)
  )->fetchAll();
}

/**
 * Loads answer options for a given question revision.
 */
function _quiz_kit_load_answers($question_vid) {
  return db_query(
    'SELECT * FROM {quiz_kit_answer} WHERE question_vid = :vid ORDER BY weight, id',
    array(':vid' => $question_vid)
  )->fetchAll();
}

/**
 * Builds a form element for a single question.
 */
function _quiz_kit_question_element($question, $answers) {
  $text = db_query('SELECT question_text FROM {quiz_kit_question_revision} WHERE vid = :vid', array(':vid' => $question->vid))->fetchField();

  $element = array(
    '#type' => 'fieldset',
    '#title' => check_plain($text),
  );

  switch ($question->type) {
    case 'mc_single':
    case 'truefalse':
      $options = array();
      foreach ($answers as $a) {
        $options[$a->id] = check_plain($a->answer_text);
      }
      $element['answer'] = array(
        '#type' => 'radios',
        '#title' => t('Your answer'),
        '#options' => $options,
        '#required' => TRUE,
      );
      break;

    case 'mc_multi':
      $options = array();
      foreach ($answers as $a) {
        $options[$a->id] = check_plain($a->answer_text);
      }
      $element['answer'] = array(
        '#type' => 'checkboxes',
        '#title' => t('Select all that apply'),
        '#options' => $options,
      );
      break;

    case 'short_text':
      $element['answer'] = array(
        '#type' => 'textfield',
        '#title' => t('Your answer'),
        '#required' => TRUE,
      );
      break;
  }

  return $element;
}

/**
 * Grades a single answer. Returns TRUE if correct.
 */
function _quiz_kit_grade_answer($question, $given) {
  switch ($question->type) {
    case 'mc_single':
    case 'truefalse':
      $correct_id = db_query(
        'SELECT id FROM {quiz_kit_answer} WHERE question_vid = :vid AND is_correct = 1',
        array(':vid' => $question->vid)
      )->fetchField();
      return (int) $given === (int) $correct_id;

    case 'mc_multi':
      $correct_ids = db_query(
        'SELECT id FROM {quiz_kit_answer} WHERE question_vid = :vid AND is_correct = 1',
        array(':vid' => $question->vid)
      )->fetchCol();
      $given_ids = array_keys(array_filter((array) $given));
      sort($correct_ids);
      sort($given_ids);
      return $correct_ids == $given_ids;

    case 'short_text':
      $correct = db_query(
        'SELECT answer_text FROM {quiz_kit_answer} WHERE question_vid = :vid AND is_correct = 1',
        array(':vid' => $question->vid)
      )->fetchField();
      return mb_strtolower(trim((string) $given)) === mb_strtolower(trim((string) $correct));
  }

  return FALSE;
}

/**
 * Returns a per-node quiz setting, falling back to a default.
 *
 * Quiz settings are stored as node fields. This helper reads them via
 * field_get_items() once field definitions are in place. For now it returns
 * the supplied default.
 *
 * @todo Replace stub with field_get_items() reads once fields are defined.
 */
function _quiz_kit_node_setting($node, $setting, $default) {
  $field_name = 'field_quiz_' . $setting;
  $items = field_get_items('node', $node, $field_name);
  if ($items && isset($items[0]['value'])) {
    return $items[0]['value'];
  }
  // Fall back to global config, then hardcoded default.
  $config_key = 'default_' . $setting;
  $global = config_get('quiz_kit.settings', $config_key);
  return $global !== NULL ? $global : $default;
}
