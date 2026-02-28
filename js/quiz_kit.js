(function ($) {
  'use strict';

  /**
   * Countdown timer for timed quizzes.
   *
   * Reads data-time-limit (seconds) from .quiz-kit-timer and counts down,
   * updating .quiz-kit-countdown. Submits the form automatically when time
   * expires.
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

      function formatTime(s) {
        var m = Math.floor(s / 60);
        var sec = s % 60;
        return m + ':' + (sec < 10 ? '0' : '') + sec;
      }

      $display.text(formatTime(seconds));

      var interval = setInterval(function () {
        seconds -= 1;
        $display.text(formatTime(seconds));

        if (seconds <= 0) {
          clearInterval(interval);
          // Auto-submit the quiz form.
          $timer.closest('form').find('[type="submit"]').first().trigger('click');
        }
      }, 1000);
    }
  };

}(jQuery));
