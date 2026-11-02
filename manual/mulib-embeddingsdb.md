# Embeddings

Define `MULIB_IMPL_EMBEDDINGSDB`; link `-lm` where required. Set dimension before
use. Put copies finite vectors; nearest performs exact Euclidean linear search,
breaking ties by smallest id. Distances may overflow to infinity. No ANN index.
[Example](../examples/modules/embeddingsdb.c), [contracts](storage-server.md).
