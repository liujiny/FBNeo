/* The SDK CRT declared these as BSS arrays. Bind to the real table. */
__init_array_start = ADDR(.init_array);
__init_array_end = ADDR(.init_array) + SIZEOF(.init_array);
