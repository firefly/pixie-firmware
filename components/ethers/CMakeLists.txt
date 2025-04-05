cmake_minimum_required(VERSION 3.16)

idf_component_register(
  SRCS
    "src/ecc.c"
    "src/keccak.c"
    "src/sha2.c"
    "src/rlp.c"
    "src/tx.c"

  INCLUDE_DIRS
    "include"
)
