from __future__ import annotations
from dataclasses import dataclass

import json
import pkg_resources


@dataclass
class ProfanityMasker:
    def __init__(self, *, bad_words: dict[str, dict[str, str]] | None = None) -> None:
        self.bad_words = (
            bad_words
            if bad_words is not None
            else json.loads(pkg_resources.resource_string(__name__, "bad_words.json"))
        )

    def mask(self, content: str) -> str:
        for bad_word in self.bad_words["bad_words"]:
            if bad_word in content:
                content = content.replace(bad_word, "***")
        return content
