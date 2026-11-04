# ASCII Soundex

Define `MULIB_IMPL_PHONETIC_UTILS`.
[Example](../examples/modules/phonetic_utils.c).

`mulib_phonetic_soundex` writes four characters and a terminator to a caller's
five-byte buffer. It retains the uppercased first letter, encodes consonants,
suppresses adjacent repeated codes, pads with zeroes and truncates to length
four. Vowels/Y separate codes; H/W do not. Entire input is validated before
output changes. Require a nonempty NUL-terminated ASCII-letter-only word;
NULL, punctuation, digits and non-ASCII bytes return 0 without output mutation.
No allocations. Soundex is a lossy English-name heuristic, not a pronunciation
model or multilingual phonetic transcription.
