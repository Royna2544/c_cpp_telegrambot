use std::num::NonZero;

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
    last_time: std::time::Instant,
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
            last_time: std::time::Instant::now() - interval,
            interval,
        }
    }

    /// Checks if enough time has elapsed since the last action.
    ///
    /// Returns `true` if the action is allowed (enough time has elapsed),
    /// `false` otherwise. When returning `true`, the internal timer is updated.
    pub fn check(&mut self) -> bool {
        let now = std::time::Instant::now();
        let elapsed = now.duration_since(self.last_time);
        if elapsed < self.interval {
            return false;
        }
        self.last_time = std::time::Instant::now();
        true
    }
}
