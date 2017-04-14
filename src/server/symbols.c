
/*
 * symbols.c -- Implements functions handling symbols conversion,
 *              including punctuation, for Speech Dispatcher
 *
 * Copyright (C) 2001,2002,2003, 2007, 2017 Brailcom, o.p.s
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/* Based off NVDA's symbols replacement code (GPLv2+):
 * https://github.com/nvaccess/nvda/blob/master/source/characterProcessing.py */

/*
 * TODO:
 * - play nice with SSML.  That might not be easy.
 * - support NUL byte representation.  However, they aren't properly handled
 *   in the rest of SPD, so it' snot so important.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <spd_utils.h>
#include "symbols.h"

/* Speech symbols levels */
typedef enum {
	SYMLVL_INVALID = -1,
	SYMLVL_NONE = 0,
	SYMLVL_SOME = 100,
	SYMLVL_MOST = 200,
	SYMLVL_ALL = 300,
	SYMLVL_CHAR = 1000
} SymLvl;

/* Speech symbol preserve modes */
typedef enum {
	SYMPRES_INVALID = -1,
	SYMPRES_NEVER = 0,
	SYMPRES_ALWAYS = 1,
	SYMPRES_NOREP = 2
} SymPresMode;

/* Represents a single symbol, and how it should be handled. */
typedef struct {
	char *identifier;
	char *pattern;
	char *replacement;
	SymLvl level;
	SymPresMode preserve;
	char *display_name;
} SpeechSymbol;

/* Represents all symbols in a symbols file.
 * This is roughly an internal representation of the symbols files. */
typedef struct {
	/* FIXME: should tables be ordered? */
	/* table of identifier(string):pattern(string) */
	GHashTable *complex_symbols;
	/* table of identifier(string):symbol(SpeechSymbol) */
	GHashTable *symbols;
} SpeechSymbols;

/* Describes a name->value translation for a field that should be loaded
 * as an integer. */
typedef struct {
	const char *name;
	int value;
} IntFieldDesc;

/* Represents a loaded and cached set of symbols in a usable form */
typedef struct {
	GRegex *regex; /* compiled regular expression for parsing input */
	/* Table of identifier(string):symbol(SpeechSymbol).
	 * Indexes are pointers to symbol->identifier. */
	GHashTable *symbols;
	/* list of SpeechSymbol (weak pointers to entries in @c symbols) */
	GList *complex_list;

	/* State */
	SymLvl level;
} SpeechSymbolProcessor;

/* Map of locale code to arbitrary data. */
typedef GHashTable LocaleMap;
typedef gpointer (*LocaleMapCreateDataFunc) (const gchar *locale);

/* globals for caching */

/* Map of SpeechSymbols, indexed by their locale */
static LocaleMap *G_symbols_dicts = NULL;
/* Map of SpeechSymbolProcessor, indexed by their locale */
static LocaleMap *G_processors = NULL;


/*----------------------------- Locale data map -----------------------------*/

static LocaleMap *locale_map_new(GDestroyNotify value_destroy)
{
	return g_hash_table_new_full(g_str_hash, g_str_equal, g_free, value_destroy);
}

static void locale_map_free(LocaleMap *map)
{
	g_hash_table_destroy(map);
}

/* Fetches or creates a locale item for the map.
 * If @c locale contains a country and data for the whole locale is not found,
 * tries to load the data for the language alone. */
static gpointer locale_map_fetch(LocaleMap *map, const gchar *locale,
				 LocaleMapCreateDataFunc create)
{
	guint i;

	for (i = 0; i < 2; i++) {
		gpointer value;

		if (i == 0) {
			value = g_hash_table_lookup(map, locale);
		} else {
			gchar **parts = g_strsplit_set(locale, "_-", 2);
			if (!parts[0] || !parts[1]) {
				/* no delimiters, no need to try again */
				g_strfreev(parts);
				continue;
			}
			value = g_hash_table_lookup(map, parts[0]);
			g_strfreev(parts);
		}
		if (value)
			return value;
		/* try to create */
		value = create(locale);
		if (value) {
			g_hash_table_insert(map, g_strdup(locale), value);
			return value;
		}
	}

	return NULL;
}

/*----------------- Speech symbol representation and loading ----------------*/

static SpeechSymbol *speech_symbol_new(void)
{
	SpeechSymbol *sym = g_slice_alloc(sizeof *sym);

	sym->identifier = NULL;
	sym->pattern = NULL;
	sym->replacement = NULL;
	sym->level = SYMLVL_INVALID;
	sym->preserve = SYMPRES_INVALID;
	sym->display_name = NULL;

	return sym;
}

static void speech_symbol_free(SpeechSymbol *sym)
{
	g_free(sym->identifier);
	g_free(sym->pattern);
	g_free(sym->replacement);
	g_free(sym->display_name);
	g_slice_free1(sizeof *sym, sym);
}

/* checks whether the line should be skipped: either blank or commented */
static int skip_line(const char *line)
{
	if (*line == '#')
		return 1;
	while (g_ascii_isspace(*line))
		line++;
	return *line == 0;
}

/* strips \r and \n at the end of a single line buffer */
static void strip_newline(char *line)
{
	while (*line && *line != '\r' && *line != '\n')
		line++;
	*line = 0;
}

/* Loads an "identifier\tpattern" line into complex_symbols */
static int speech_symbols_load_complex_symbol(SpeechSymbols *ss, const char *line)
{
	char **parts = g_strsplit(line, "\t", 2);

	if (g_strv_length(parts) != 2) {
		g_strfreev(parts);
		return -1;
	}

	g_hash_table_insert(ss->complex_symbols, parts[0], parts[1]);
	g_free(parts);

	return 0;
}

/* Finds the entry in @p map that corresponds to @p name, and put its value
 * into the integer pointer to by @p value */
static int speech_symbols_load_int_field(IntFieldDesc *map, guint map_len,
					 const char *name, int *value)
{
	guint i;

	for (i = 0; i < map_len; i++) {
		if (strcmp(map[i].name, name) == 0) {
			*value = map[i].value;
			return 0;
		}
	}

	return -1;
}

/* Loads a symbol line into symbols
 * syntax is:
 *   identifier "\t" replacement [ "\t" level [ "\t" preserve ] [ "\t#" comment ] */
static int speech_symbols_load_symbol(SpeechSymbols *ss, const char *line)
{
	char **parts = g_strsplit(line, "\t", -1);
	guint len = g_strv_length(parts);
	char *display_name = NULL;
	char *identifier = NULL;
	char *replacement = NULL;
	int level = SYMLVL_INVALID;
	int pres_mode = SYMPRES_INVALID;
	SpeechSymbol *sym;

	/* last field, if commented: display name */
	if (parts[len - 1][0] == '#') {
		/* Regardless of how many fields there are,
		 * if the last field is a comment, it is the display name. */
		const char *p;

		display_name = parts[len - 1];
		parts[--len] = NULL;

		p = display_name + 1;
		while (g_ascii_isspace(*p))
			p++;
		memmove(display_name, p, strlen(p) + 1);
	}

	/* 4th field (optional): preserve */
	if (len > 3) {
		IntFieldDesc map[] = {
			{ "-",		SYMPRES_NEVER },
			{ "never",	SYMPRES_NEVER },
			{ "always",	SYMPRES_ALWAYS },
			{ "norep",	SYMPRES_NOREP },
		};

		if (speech_symbols_load_int_field(map, G_N_ELEMENTS(map),
						  parts[3], &pres_mode) < 0)
			goto err;
	}

	/* 3rd field (optional): level */
	if (len > 2) {
		IntFieldDesc map[] = {
			{ "-",		SYMLVL_NONE },
			{ "none",	SYMLVL_NONE },
			{ "some",	SYMLVL_SOME },
			{ "most",	SYMLVL_MOST },
			{ "all",	SYMLVL_ALL },
			{ "char",	SYMLVL_CHAR },
		};

		if (speech_symbols_load_int_field(map, G_N_ELEMENTS(map),
						  parts[2], &level) < 0)
			goto err;
	}

	/* missing required fields */
	if (len < 2 || !parts[0] || !parts[0][0])
		goto err;

	/* 2nd field: replacement */
	if (strcmp(parts[1], "-") == 0)
		replacement = NULL;
	else
		replacement = g_strdup(parts[1]);

	/* 1st field: identifier */
	if (parts[0][0] == '\\' && parts[0][1]) {
		identifier = g_strdup(parts[0] + 1);
		switch (identifier[0]) {
		case '0': identifier[0] = '\0'; break; /* FIXME: len */
		case 't': identifier[0] = '\t'; break;
		case 'n': identifier[0] = '\n'; break;
		case 'r': identifier[0] = '\r'; break;
		case 'f': identifier[0] = '\f'; break;
		case 'v': identifier[0] = '\v'; break;
		case '#':
		case '\\':
			/* nothing to do */
			break;
		}
	} else
		identifier = g_strdup(parts[0]);

	sym = speech_symbol_new();
	sym->identifier = identifier;
	sym->replacement = replacement;
	sym->level = level;
	sym->preserve = pres_mode;
	sym->display_name = display_name;

	g_hash_table_insert(ss->symbols, sym->identifier, sym);

	g_strfreev(parts);

	return 0;

err:
	g_free(display_name);
	g_free(identifier);
	g_free(replacement);
	g_strfreev(parts);

	return -1;
}

/* Loads a symbols.dic file into @p ss */
static int speech_symbols_load(SpeechSymbols *ss, const char *filename, gboolean allow_complex)
{
	FILE *fp;
	char *line = NULL;
	size_t n = 0;
	unsigned char bom[3];
	int (*handler) (SpeechSymbols *, const char *) = NULL;

	fp = fopen(filename, "r");
	if (!fp)
		return -1;

	/* skip UTF-8 BOM if present */
	if (fread(bom, sizeof *bom, sizeof bom, fp) != sizeof bom ||
	    bom[0] != 0xEF || bom[1] != 0xBB || bom[2] != 0xBF)
		fseek(fp, 0, SEEK_SET);

	while (spd_getline(&line, &n, fp) >= 0) {
		if (skip_line(line))
			continue;
		strip_newline(line);

		if (allow_complex && strcmp(line, "complexSymbols:") == 0) {
			handler = speech_symbols_load_complex_symbol;
		} else if (strcmp(line, "symbols:") == 0) {
			handler = speech_symbols_load_symbol;
		} else if (!handler || handler(ss, line) < 0) {
			MSG2(1, "symbols", "Invalid line in file %s: %s",
			     filename, line);
		}
	}

	g_free(line);
	fclose(fp);

	return 0;
}

static void speech_symbols_free(SpeechSymbols *ss)
{
	g_hash_table_destroy(ss->complex_symbols);
	g_hash_table_destroy(ss->symbols);
	g_free(ss);
}

static SpeechSymbols *speech_symbols_new(const gchar *locale)
{
	SpeechSymbols *ss = g_malloc(sizeof *ss);
	gchar *path = g_build_filename(LOCALE_DATA, locale, "symbols.dic", NULL);

	ss->complex_symbols = g_hash_table_new_full(g_str_hash, g_str_equal,
						    g_free, g_free);
	ss->symbols = g_hash_table_new_full(g_str_hash, g_str_equal,
					    NULL /* key is a member of value */,
					    (GDestroyNotify) speech_symbol_free);

	MSG2(5, "symbols", "Trying to load symbols.dic for '%s' from '%s'", locale, path);
	if (speech_symbols_load(ss, path, TRUE) < 0) {
		speech_symbols_free(ss);
		ss = NULL;
	}
	g_free(path);

	return ss;
}

static SpeechSymbols *get_locale_speech_symbols(const gchar *locale)
{
	if (!G_symbols_dicts) {
		G_symbols_dicts = locale_map_new((GDestroyNotify) speech_symbols_free);
	}

	return locale_map_fetch(G_symbols_dicts, locale,
				(LocaleMapCreateDataFunc) speech_symbols_new);
}

/*------------------ Speech symbol compilation & processing -----------------*/

/* sort function sorting strings by length, longest first */
static gint list_sort_string_longest_first(gconstpointer a, gconstpointer b)
{
	return strlen(b) - strlen(a);
}

static void speech_symbols_processor_free(SpeechSymbolProcessor *ssp)
{
	if (ssp->regex)
		g_regex_unref(ssp->regex);
	g_list_free(ssp->complex_list);
	if (ssp->symbols)
		g_hash_table_unref(ssp->symbols);
	g_free(ssp);
}

/* Loads and compiles speech symbols conversions */
static SpeechSymbolProcessor *speech_symbols_processor_new(const char *locale)
{
	SpeechSymbolProcessor *ssp = NULL;
	SpeechSymbols *ss;
	GHashTableIter iter;
	gpointer key, value;
	GString *characters = g_string_new(NULL);
	GList *multi_chars_list = NULL;
	gchar *escaped;
	GString *escaped_multi;
	GString *pattern;
	GError *error = NULL;
	GSList *sources = NULL;

	/* TODO: load user custom symbols? */
	ss = get_locale_speech_symbols(locale);
	if (!ss) {
		MSG2(1, "symbols", "Failed to load symbols");
		return NULL;
	}

	sources = g_slist_append(sources, ss);
	/* Always use English as a base. */
	if (strcmp(locale, "en") != 0) {
		ss = get_locale_speech_symbols("en");
		if (ss)
			sources = g_slist_append(sources, ss);
	}

	ssp = g_malloc(sizeof *ssp);
	/* The computed symbol information from all sources. */
	ssp->symbols = g_hash_table_new_full(g_str_hash, g_str_equal,
					     NULL,
					     (GDestroyNotify) speech_symbol_free);
	/* An indexable list of complex symbols for use in building/executing the regexp. */
	ssp->complex_list = NULL;

	/* Add all complex symbols first, as they take priority. */
	for (GSList *node = sources; node; node = node->next) {
		SpeechSymbols *syms = node->data;

		g_hash_table_iter_init(&iter, syms->complex_symbols);
		while (g_hash_table_iter_next(&iter, &key, &value)) {
			SpeechSymbol *sym;

			if (g_hash_table_contains(ssp->symbols, key)) {
				/* Already defined */
				continue;
			}

			sym = speech_symbol_new();
			sym->identifier = g_strdup(key);
			sym->pattern = g_strdup(value);
			g_hash_table_insert(ssp->symbols, sym->identifier, sym);
			ssp->complex_list = g_list_append(ssp->complex_list, sym);
		}
	}

	/* Supplement the data for complex symbols and add all simple symbols. */
	for (GSList *node = sources; node; node = node->next) {
		SpeechSymbols *syms = node->data;

		g_hash_table_iter_init(&iter, syms->symbols);
		while (g_hash_table_iter_next(&iter, &key, &value)) {
			const SpeechSymbol *source_sym = value;
			SpeechSymbol *sym;

			sym = g_hash_table_lookup(ssp->symbols, key);
			if (!sym) {
				if (((char *)key)[0] == 0) {
					/* FIXME: NOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO
					 * A NULL byte, I'm so fucked up.  TODO: handle length properly. */
					MSG2(1, "symbols", "WARNING: not including NUL byte, not supported yet");
					continue;
				}

				/* This is a new simple symbol.
				 * (All complex symbols have already been added.) */
				sym = speech_symbol_new();
				sym->identifier = g_strdup(key);
				g_hash_table_insert(ssp->symbols, sym->identifier, sym);
				/* FIXME: should we use Unicode characters? */
				if (strlen(sym->identifier) == 1) {
					g_string_append_c(characters, sym->identifier[0]);
				} else {
					multi_chars_list = g_list_prepend(multi_chars_list, sym->identifier);
				}
			}
			/* If fields weren't explicitly specified, inherit the value from later sources. */
			if (sym->replacement == NULL)
				sym->replacement = g_strdup(source_sym->replacement);
			if (sym->level == SYMLVL_INVALID)
				sym->level = source_sym->level;
			if (sym->preserve == SYMPRES_INVALID)
				sym->preserve = source_sym->preserve;
			if (sym->display_name == NULL)
				sym->display_name = g_strdup(source_sym->display_name);
		}
	}

	/* Set defaults for any fields not explicitly set. */
	g_hash_table_iter_init(&iter, ssp->symbols);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		SpeechSymbol *sym = value;

		if (!sym->replacement) {
			/* Symbols without a replacement specified are useless. */
			MSG2(2, "symbols", "Replacement not defined "
					   "in locale %s for symbol: %s",
					   locale, sym->identifier);
			ssp->complex_list = g_list_remove(ssp->complex_list, sym);
			g_hash_table_iter_remove(&iter);
			continue;
		}
		if (sym->level == SYMLVL_INVALID)
			sym->level = SYMLVL_ALL;
		if (sym->preserve == SYMPRES_INVALID)
			sym->preserve = SYMPRES_NEVER;
		if (sym->display_name == NULL)
			sym->display_name = g_strdup(sym->identifier);
	}

	/* build the regex. */

	/* Make characters into a regexp character set. */
	escaped = g_regex_escape_string(characters->str, characters->len);
	g_string_truncate(characters, 0);
	g_string_append_printf(characters, "[%s]", escaped);
	g_free(escaped);

	/* The simple symbols must be ordered longest first so that the longer symbols will match.*/
	multi_chars_list = g_list_sort(multi_chars_list, list_sort_string_longest_first);

	/* TODO: check the syntax is compatible with GLib */
	pattern = g_string_new(NULL);
	/* Strip repeated spaces from the end of the line to stop them from being picked up by repeated. */
	g_string_append(pattern, "(?P<rstripSpace>  +$)");
	/* Repeated characters: more than 3 repeats. */
	g_string_append_c(pattern, '|');
	g_string_append_printf(pattern, "(?P<repeated>(?P<repTmp>%s)(?P=repTmp){3,})", characters->str);
	/* Complex symbols.
	 * Each complex symbol has its own named group so we know which symbol matched. */
	guint i = 0;
	for (GList *node = ssp->complex_list; node; node = node->next, i++) {
		SpeechSymbol *sym = node->data;
		g_string_append_c(pattern, '|');
		g_string_append_printf(pattern, "(?P<c%u>%s)", i, sym->pattern);
	}
	/* Simple symbols.
	 * These are all handled in one named group.
	 * Because the symbols are just text, we know which symbol matched just by looking at the matched text. */
	escaped_multi = g_string_new(NULL);
	for (GList *node = multi_chars_list; node; node = node->next) {
		escaped = g_regex_escape_string(node->data, -1);
		if (escaped_multi->len > 0)
			g_string_append_c(escaped_multi, '|');
		g_string_append(escaped_multi, escaped);
		g_free(escaped);
	}
	g_string_append_c(pattern, '|');
	g_string_append_printf(pattern, "(?P<simple>%s|%s)",
			       escaped_multi->str, characters->str);
	g_string_free(escaped_multi, TRUE);

	MSG2(5, "symbols", "building regex: %s", pattern->str);
	ssp->regex = g_regex_new(pattern->str, G_REGEX_OPTIMIZE, 0, &error);
	if (!ssp->regex) {
		/* if regex compilation failed, bail out */
		MSG2(1, "symbols", "ERROR compiling regular expression: %s. "
				   "This is likely due to an invalid complex "
				   "symbol regular expression in locale %s.",
				   error->message, locale);
		g_error_free(error);
		speech_symbols_processor_free(ssp);
		ssp = NULL;
	}

	g_string_free(pattern, TRUE);
	g_string_free(characters, TRUE);
	g_list_free(multi_chars_list);

	return ssp;
}

/* Fetch a named group that matched.
 * FIXME: handle empty groups? (e.g. with only lookaheads/lookbehinds) */
static gchar *fetch_named_matching(const GMatchInfo *match_info, const gchar *name)
{
	gchar *capture = g_match_info_fetch_named(match_info, name);

	if (capture && !*capture) {
		g_free(capture);
		capture = NULL;
	}

	return capture;
}

/* Regular expression callback for applying replacements */
static gboolean regex_eval(const GMatchInfo *match_info, GString *result, gpointer user_data)
{
	SpeechSymbolProcessor *ssp = user_data;
	gchar *capture;

	/* FIXME: Python regex API allows to find the name of the group that
	 *        matched.  As GRegex doesn't have that, what we do here is try
	 *        and fetch the groups we know, and see if they matched.
	 *        This is not very optimal, but how can we avoid that? */

	if ((capture = fetch_named_matching(match_info, "rstripSpace"))) {
		MSG2(5, "symbols", "replacing <rstripSpace>");
		/* nothing to do */
	} else if ((capture = fetch_named_matching(match_info, "repeated"))) {
		/* Repeated character. */
		char ch[2] = { capture[0], 0 };
		SpeechSymbol *sym = g_hash_table_lookup(ssp->symbols, ch);

		MSG2(5, "symbols", "replacing <repeated>");

		/* this should never happen, but be on the safe side and check it */
		if (!sym)
			goto symbol_error;

		if (ssp->level >= sym->level) {
			g_string_append_printf(result, " %lu %s ", strlen(capture), sym->replacement);
		} else {
			g_string_append_c(result, ' ');
		}
	} else {
		SpeechSymbol *sym = NULL;
		const gchar *suffix;

		/* One of the defined symbols. **/
		if ((capture = fetch_named_matching(match_info, "simple"))) {
			/* Simple symbol. */
			sym = g_hash_table_lookup(ssp->symbols, capture);
			MSG2(5, "symbols", "replacing <simple>");
		} else {
			/* Complex symbol. */
			guint i = 0;

			for (GList *node = ssp->complex_list; !sym && node; node = node->next, i++) {
				gchar *group_name = g_strdup_printf("c%u", i);

				if ((capture = fetch_named_matching(match_info, group_name))) {
					sym = node->data;
					MSG2(5, "symbols", "replacing <%s> (complex symbol)", group_name);
				}
				g_free(group_name);
			}
		}

		/* this should never happen, but be on the safe side and check it */
		if (!sym)
			goto symbol_error;

		MSG2(5, "symbols", "replacing sym |%s| (lvl=%d, preserve=%d)",
		     sym->identifier, sym->level, sym->preserve);

		if (sym->preserve == SYMPRES_ALWAYS ||
		    (sym->preserve == SYMPRES_NOREP && ssp->level < sym->level))
			suffix = capture;
		else
			suffix = " ";

		if (ssp->level >= sym->level && sym->replacement) {
			g_string_append_printf(result, " %s%s", sym->replacement, suffix);
		} else {
			g_string_append(result, suffix);
		}
	}

	g_free(capture);

	return FALSE;

	symbol_error: {
		gint start = -1, end = -1;
		gchar *group_0;

		g_match_info_fetch_pos(match_info, 0, &start, &end);
		group_0 = g_match_info_fetch(match_info, 0);
		MSG2(1, "symbols", "WARNING: no symbol for match |%s| (at %d..%d), this shouldn't happen.",
		     group_0, start, end);

		g_free(group_0);
		g_free(capture);
		return FALSE;
	}
}

/* Processes some input and convert symbols in it */
static gchar *speech_symbols_processor_process_text(SpeechSymbolProcessor *ssp, const gchar *text, SymLvl level)
{
	gchar *processed;
	GError *error = NULL;

	ssp->level = level;
	processed = g_regex_replace_eval(ssp->regex, text, -1, 0, 0, regex_eval, ssp, &error);
	if (!processed) {
		MSG2(1, "symbols", "ERROR applying regex: %s", error->message);
		g_error_free(error);
	}

	return processed;
}

/* Gets a possibly cached processor for the given locale */
static SpeechSymbolProcessor *get_locale_speech_symbols_processor(const gchar *locale)
{
	if (!G_processors) {
		G_processors = locale_map_new((GDestroyNotify) speech_symbols_processor_free);
	}

	return locale_map_fetch(G_processors, locale,
				(LocaleMapCreateDataFunc) speech_symbols_processor_new);
}

/*----------------------------------- API -----------------------------------*/

/* Process some text, converting symbols according to desired pronunciation. */
static gchar *process_speech_symbols(const gchar *locale, const gchar *text, SymLvl level)
{
	SpeechSymbolProcessor *ssp;

	ssp = get_locale_speech_symbols_processor(locale);
	/* fallback to English if there's no processor for the locale */
	if (!ssp && g_str_has_prefix(locale, "en") && strchr("_-", locale[2]))
		ssp = get_locale_speech_symbols_processor("en");
	if (!ssp)
		return NULL;

	return speech_symbols_processor_process_text(ssp, text, level);
}

void insert_symbols(TSpeechDMessage *msg)
{
	gchar *processed;
	SymLvl level;

	if (msg->settings.ssml_mode == SPD_DATA_SSML) {
		/* FIXME: we need not ever speak the SSML syntax */
		MSG2(2, "symbols", "Converting symbols in SSML is not supported yet");
		return;
	}

	switch (msg->settings.msg_settings.punctuation_mode) {
	case SPD_PUNCT_ALL: level = SYMLVL_ALL; break;
	case SPD_PUNCT_NONE: level = SYMLVL_NONE; break;
	case SPD_PUNCT_SOME: level = SYMLVL_SOME; break;
	}

	processed = process_speech_symbols(msg->settings.msg_settings.voice.language,
					   msg->buf, level);
	if (processed) {
		MSG2(5, "symbols", "before: |%s|", msg->buf);
		g_free(msg->buf);
		msg->buf = processed;
		MSG2(5, "symbols", "after: |%s|", msg->buf);
		/* if we performed the replacement, don't let the module speak it again */
		msg->settings.msg_settings.punctuation_mode = SPD_PUNCT_NONE;
	}
}
