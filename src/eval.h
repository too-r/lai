#pragma once

size_t lai_eval_integer(uint8_t *object, uint64_t *integer);
size_t lai_parse_pkgsize(uint8_t *, size_t *);
int lai_eval_package(lai_object_t *, size_t, lai_object_t *);
int lai_is_name(char);
void lai_eval_operand(lai_object_t *, lai_state_t *, uint8_t *);
