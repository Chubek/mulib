/* Original deterministic fixtures; no downloaded corpora or training data. */
#ifndef MULIB_STATISTICAL_TEXT_FIXTURES_H
#define MULIB_STATISTICAL_TEXT_FIXTURES_H
#include <stddef.h>
static const double mulib_fixture_initial[] = {0.6, 0.4};
static const double mulib_fixture_transition[] = {0.7, 0.3, 0.4, 0.6};
static const double mulib_fixture_emission[] = {0.5, 0.4, 0.1, 0.1, 0.3, 0.6};
static const size_t mulib_fixture_observations[] = {0, 1, 2};
static const char mulib_fixture_corpus[] = "Rain rain, SUN! Rain and wind; sun, cloud.";
static const char *const mulib_fixture_words[] = {"book", "books", "cake", "boo", "cape", "boon", "cook", "cart", ""};
#endif
