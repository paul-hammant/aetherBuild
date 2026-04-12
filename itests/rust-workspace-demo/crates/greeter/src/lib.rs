use serde::{Serialize, Deserialize};

#[derive(Serialize, Deserialize, Debug)]
pub struct Greeting {
    pub message: String,
}

pub fn greet(name: &str) -> Greeting {
    Greeting {
        message: format!("Hello, {}!", name),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_greet() {
        let g = greet("world");
        assert_eq!(g.message, "Hello, world!");
    }
}
