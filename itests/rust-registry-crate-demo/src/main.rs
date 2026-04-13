use serde_json::json;

fn main() {
    let v = json!({ "greeting": "hello", "language": "rust" });
    println!("{}", v);
}
