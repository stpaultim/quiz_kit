(function ($) {
  'use strict';

  /**
   * Countdown timer for timed quizzes.
   *
   * Reads data-time-limit (seconds) from .quiz-kit-timer and counts down,
   * updating .quiz-kit-countdown. When time expires, clicks the hidden
   * timeout submit button to auto-submit the quiz.
   */
  Backdrop.behaviors.quizKitTimer = {
    attach: function (context, settings) {
      var $timer = $('.quiz-kit-timer', context).once('quiz-kit-timer');
      if (!$timer.length) {
        return;
      }

      var seconds = parseInt($timer.data('time-limit'), 10);
      if (!seconds || seconds <= 0) {
        return;
      }

      var $display = $timer.find('.quiz-kit-countdown');
      var warned = false;

      function formatTime(s) {
        var m = Math.floor(s / 60);
        var sec = s % 60;
        return m + ':' + (sec < 10 ? '0' : '') + sec;
      }

      $display.text(formatTime(seconds));

      var interval = setInterval(function () {
        seconds -= 1;
        $display.text(formatTime(seconds));

        // Visual warning when under 60 seconds.
        if (seconds <= 60 && !warned) {
          warned = true;
          $timer.addClass('quiz-kit-timer-warning');
        }

        if (seconds <= 0) {
          clearInterval(interval);
          // Click the hidden timeout button to submit and grade the quiz.
          var $timeout = $timer.closest('form').find('.quiz-kit-timeout-submit');
          if ($timeout.length) {
            $timeout.trigger('click');
          }
          else {
            // Fallback: click the last visible submit button (Submit quiz).
            $timer.closest('form').find('[type="submit"]:visible').last().trigger('click');
          }
        }
      }, 1000);
    }
  };

}(jQuery));
