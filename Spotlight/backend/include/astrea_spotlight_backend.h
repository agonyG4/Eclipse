#ifndef ASTREA_SPOTLIGHT_BACKEND_H
#define ASTREA_SPOTLIGHT_BACKEND_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AstreaSpotlightBackend AstreaSpotlightBackend;

AstreaSpotlightBackend *astrea_spotlight_backend_create(const char *astrea_root,
                                                        const char *locale,
                                                        char **error_out);

void astrea_spotlight_backend_destroy(AstreaSpotlightBackend *backend);

int astrea_spotlight_backend_reload(AstreaSpotlightBackend *backend,
                                    char **error_out);

char *astrea_spotlight_backend_search_json(AstreaSpotlightBackend *backend,
                                           const char *query,
                                           size_t limit,
                                           char **error_out);

int astrea_spotlight_backend_record_launch(AstreaSpotlightBackend *backend,
                                           const char *desktop_id,
                                           char **error_out);

int astrea_spotlight_backend_ensure_config(char **error_out);

char *astrea_spotlight_backend_watched_dirs(AstreaSpotlightBackend *backend,
                                            char **error_out);

void astrea_spotlight_backend_free_string(char *value);

#ifdef __cplusplus
}
#endif

#endif
