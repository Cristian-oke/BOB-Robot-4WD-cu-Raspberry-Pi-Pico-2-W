# converteste un fisier binar in sursa C linkuita
# folosit de CMakeLists.txt pentru a embedded index.html in firmware

file(READ "${INPUT_FILE}" content HEX)
string(LENGTH "${content}" hex_len)

set(out "/* AUTO-GENERAT — NU EDITA */\n")
set(out "${out}#include <stddef.h>\n\n")
set(out "${out}const char _binary_${SYMBOL}_start[] = {\n  ")

set(i 0)
set(col 0)
while(i LESS hex_len)
    math(EXPR end "${i} + 2")
    string(SUBSTRING "${content}" ${i} 2 byte_hex)
    set(out "${out}0x${byte_hex},")
    math(EXPR col "${col} + 1")
    if(col EQUAL 16)
        set(out "${out}\n  ")
        set(col 0)
    else()
        set(out "${out} ")
    endif()
    math(EXPR i "${i} + 2")
endwhile()

set(out "${out}0x00\n};\n\n")
math(EXPR byte_len "${hex_len} / 2")
set(out "${out}const char _binary_${SYMBOL}_end[] = \"\";\n")
set(out "${out}const size_t _binary_${SYMBOL}_size = ${byte_len};\n")

file(WRITE "${OUTPUT_FILE}" "${out}")
message(STATUS "Embedded: ${INPUT_FILE} → ${OUTPUT_FILE} (${byte_len} bytes)")
