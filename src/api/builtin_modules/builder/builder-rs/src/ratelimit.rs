use std::num::NonZero;

pub struct RateLimit {
    last_time: std::time::Instant,
    interval: std::time::Duration,
}

impl RateLimit {
    pub fn new(min_sec_between: NonZero<u64>) -> Self {
        let interval = std::time::Duration::from_secs(min_sec_between.get());
        RateLimit {
            last_time: std::time::Instant::now() - interval,
            interval,
        }
    }

    // Returns true if the action is allowed, false otherwise
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
