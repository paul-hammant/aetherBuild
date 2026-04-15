import sys

from chatapp.contentbuilder.builder import ContentBuilder


def print_content(content: str) -> None:
    print(ContentBuilder().build(content))


if __name__ == "__main__":
    print_content(sys.argv[1])
