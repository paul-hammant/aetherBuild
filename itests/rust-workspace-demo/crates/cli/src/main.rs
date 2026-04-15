use clap::Parser;
use greeter::greet;

#[derive(Parser)]
#[command(name = "greet")]
struct Args {
    name: String,
}

fn main() {
    let args = Args::parse();
    let g = greet(&args.name);
    println!("{}", serde_json::to_string_pretty(&g).unwrap());
}
