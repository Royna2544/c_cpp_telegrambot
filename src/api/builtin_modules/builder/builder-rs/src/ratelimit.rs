use std::{cell::Cell, num::NonZero};

/// A simple rate limiter that ensures a minimum time interval between actions.
///
/// # Example
///
/// ```
/// use std::num::NonZero;
///
/// let mut rate_limit = RateLimit::new(NonZero::new(5).unwrap());
/// if rate_limit.check() {
///     // Perform rate-limited action
/// }
/// ```
pub struct RateLimit {
    last_time: Cell<std::time::Instant>,
    interval: std::time::Duration,
}

impl RateLimit {
    /// Creates a new rate limiter with the specified minimum seconds between actions.
    ///
    /// # Arguments
    ///
    /// * `min_sec_between` - Minimum number of seconds that must elapse between actions
    pub fn new(min_sec_between: NonZero<u64>) -> Self {
        let interval = std::time::Duration::from_secs(min_sec_between.get());
        RateLimit {
            last_time: Cell::new(std::time::Instant::now() - interval),
            interval,
        }
    }

    /// Checks if enough time has elapsed since the last action.
    ///
    /// Returns `true` if the action is allowed (enough time has elapsed),
    /// `false` otherwise. When returning `true`, the internal timer is updated.
    #[must_use]
    pub fn check(&self) -> bool {
        let now = std::time::Instant::now();
        let elapsed = now.duration_since(self.last_time.get());
        if elapsed < self.interval {
            return false;
        }
        self.last_time.set(std::time::Instant::now());
        true
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::thread::sleep;
    use std::time::Duration;

    #[test]
    fn test_ratelimit_initial_check_passes() {
        // The first check should always pass because we initialize
        // last_time to (now - interval)
        let rl = RateLimit::new(NonZero::new(1).unwrap());
        assert!(rl.check());
    }

    #[test]
    fn test_ratelimit_blocks_immediate_second_call() {
        let rl = RateLimit::new(NonZero::new(1).unwrap());
        assert!(rl.check()); // First call passes
        assert!(!rl.check()); // Second immediate call should be blocked
    }

    #[test]
    fn test_ratelimit_allows_after_interval() {
        // Use a very short interval for testing
        let rl = RateLimit::new(NonZero::new(1).unwrap());
        assert!(rl.check()); // First call passes

        // Wait for the interval to pass
        sleep(Duration::from_secs(1));

        // Now the check should pass again
        assert!(rl.check());
    }

    #[test]
    fn test_ratelimit_multiple_blocked_attempts() {
        let rl = RateLimit::new(NonZero::new(2).unwrap());
        assert!(rl.check()); // First call passes

        // Multiple immediate attempts should all be blocked
        assert!(!rl.check());
        assert!(!rl.check());
        assert!(!rl.check());
    }
}
