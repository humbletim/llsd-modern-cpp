# Modern C++ LLSD Library (llsd_modern.hpp)

This is a standalone, header-only C++17 library for serializing and
deserializing Linden Lab Structured Data (LLSD).

It is designed to be:
* **Type-Safe:** Uses `std::variant` for data storage.
* **Modern:** Uses C++17 features like `std::string_view` and `std::optional`.
* **Lightweight:** Depends only on a header-only JSON library (nlohmann/json).
* **Standalone:** Has no dependencies on the legacy Second Life viewer codebase.

## Implementation Note

This library is a new C++ implementation, but its design and parsing/formatting
logic are directly inspired by and derived from the official
[python-llsd](https://github.com/secondlife/python-llsd) library by Linden Lab, Inc.
The goal of this library is to provide a modern, robust C++ equivalent to the
Python reference implementation, which can then be used to back-end other
legacy APIs.

## Licensing
This new C++ library is licensed under the MIT License (see `LICENSE`).

The original `python-llsd` library, from which this work is derived,
is also licensed under the MIT License (see `LICENSE.python-llsd`).
