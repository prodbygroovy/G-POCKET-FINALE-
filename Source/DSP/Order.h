#pragma once

#include <algorithm>
#include <array>

// L'ordine dei 4 pedali è salvato come un unico intero 0..23 (indice della permutazione),
// così è un normale parametro automatizzabile e non può mai essere "invalido".

namespace grv
{
using Order = std::array<int, 4>;

inline Order decodeOrder (int index)
{
    Order a { 0, 1, 2, 3 };
    for (int i = 0; i < index && i < 23; ++i)
        std::next_permutation (a.begin(), a.end());
    return a;
}

inline int encodeOrder (Order target)
{
    Order a { 0, 1, 2, 3 };
    for (int i = 0; i < 24; ++i)
    {
        if (a == target)
            return i;
        std::next_permutation (a.begin(), a.end());
    }
    return 0;
}
} // namespace grv
