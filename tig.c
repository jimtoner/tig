static int read_properties(FILE *pipe, const char *separators, int (*read)(char *, int, char *, int));
struct int_map {
	const char *name;
	int namelen;
	int value;
};

static int
set_from_int_map(struct int_map *map, size_t map_size,
		 int *value, const char *name, int namelen)
{

	int i;

	for (i = 0; i < map_size; i++)
		if (namelen == map[i].namelen &&
		    !strncasecmp(name, map[i].name, namelen)) {
			*value = map[i].value;
			return OK;
		}

	return ERR;
}

static char *
chomp_string(char *name)
{
	int namelen;

	while (isspace(*name))
		name++;

	namelen = strlen(name) - 1;
	while (namelen > 0 && isspace(name[namelen]))
		name[namelen--] = 0;

	return name;
}

"  -b[N], --tab-size[=N]       Set number of spaces for tab expansion\n"
/**
 * FILES
 * -----
 * '~/.tigrc'::
 *	User configuration file. See tigrc(5) for examples.
 *
 * '.git/config'::
 *	Repository config file. Read on startup with the help of
 *	git-repo-config(1).
 **/

static struct int_map color_map[] = {
#define COLOR_MAP(name) { #name, STRING_SIZE(#name), COLOR_##name }
	COLOR_MAP(DEFAULT),
	COLOR_MAP(BLACK),
	COLOR_MAP(BLUE),
	COLOR_MAP(CYAN),
	COLOR_MAP(GREEN),
	COLOR_MAP(MAGENTA),
	COLOR_MAP(RED),
	COLOR_MAP(WHITE),
	COLOR_MAP(YELLOW),
};

static struct int_map attr_map[] = {
#define ATTR_MAP(name) { #name, STRING_SIZE(#name), A_##name }
	ATTR_MAP(NORMAL),
	ATTR_MAP(BLINK),
	ATTR_MAP(BOLD),
	ATTR_MAP(DIM),
	ATTR_MAP(REVERSE),
	ATTR_MAP(STANDOUT),
	ATTR_MAP(UNDERLINE),
};
LINE(DIFF_HEADER,  "diff --git ",	COLOR_YELLOW,	COLOR_DEFAULT,	0), \
LINE(DIFF_INDEX,	"index ",	  COLOR_BLUE,	COLOR_DEFAULT,	0), \
LINE(DIFF_OLDMODE,	"old file mode ", COLOR_YELLOW,	COLOR_DEFAULT,	0), \
LINE(DIFF_NEWMODE,	"new file mode ", COLOR_YELLOW,	COLOR_DEFAULT,	0), \
LINE(DIFF_COPY_FROM,	"copy from",	  COLOR_YELLOW,	COLOR_DEFAULT,	0), \
LINE(DIFF_COPY_TO,	"copy to",	  COLOR_YELLOW,	COLOR_DEFAULT,	0), \
LINE(DIFF_RENAME_FROM,	"rename from",	  COLOR_YELLOW,	COLOR_DEFAULT,	0), \
LINE(DIFF_RENAME_TO,	"rename to",	  COLOR_YELLOW,	COLOR_DEFAULT,	0), \
LINE(DIFF_SIMILARITY,   "similarity ",	  COLOR_YELLOW,	COLOR_DEFAULT,	0), \
LINE(DIFF_DISSIMILARITY,"dissimilarity ", COLOR_YELLOW,	COLOR_DEFAULT,	0), \
LINE(DIFF_TREE,		"diff-tree ",	  COLOR_BLUE,	COLOR_DEFAULT,	0), \
LINE(MAIN_REF,     "",			COLOR_CYAN,	COLOR_DEFAULT,	A_BOLD), \


/*
 * Line-oriented content detection.
 */
	const char *name;	/* Option name. */
	int namelen;		/* Size of option name. */
	{ #type, STRING_SIZE(#type), (line), STRING_SIZE(line), (fg), (bg), (attr) }
static struct line_info *
get_line_info(char *name, int namelen)
{
	enum line_type type;
	int i;

	/* Diff-Header -> DIFF_HEADER */
	for (i = 0; i < namelen; i++) {
		if (name[i] == '-')
			name[i] = '_';
		else if (name[i] == '.')
			name[i] = '_';
	}

	for (type = 0; type < ARRAY_SIZE(line_info); type++)
		if (namelen == line_info[type].namelen &&
		    !strncasecmp(line_info[type].name, name, namelen))
			return &line_info[type];

	return NULL;
}

/*
 * User config file handling.
 */

#define set_color(color, name, namelen) \
	set_from_int_map(color_map, ARRAY_SIZE(color_map), color, name, namelen)

#define set_attribute(attr, name, namelen) \
	set_from_int_map(attr_map, ARRAY_SIZE(attr_map), attr, name, namelen)

static int   config_lineno;
static bool  config_errors;
static char *config_msg;

static int
set_option(char *opt, int optlen, char *value, int valuelen)
{
	/* Reads: "color" object fgcolor bgcolor [attr] */
	if (!strcmp(opt, "color")) {
		struct line_info *info;

		value = chomp_string(value);
		valuelen = strcspn(value, " \t");
		info = get_line_info(value, valuelen);
		if (!info) {
			config_msg = "Unknown color name";
			return ERR;
		}

		value = chomp_string(value + valuelen);
		valuelen = strcspn(value, " \t");
		if (set_color(&info->fg, value, valuelen) == ERR) {
			config_msg = "Unknown color";
			return ERR;
		}

		value = chomp_string(value + valuelen);
		valuelen = strcspn(value, " \t");
		if (set_color(&info->bg, value, valuelen) == ERR) {
			config_msg = "Unknown color";
			return ERR;
		}

		value = chomp_string(value + valuelen);
		if (*value &&
		    set_attribute(&info->attr, value, strlen(value)) == ERR) {
			config_msg = "Unknown attribute";
			return ERR;
		}

		return OK;
	}

	return ERR;
}

static int
read_option(char *opt, int optlen, char *value, int valuelen)
{
	config_lineno++;
	config_msg = "Internal error";

	optlen = strcspn(opt, "#;");
	if (optlen == 0) {
		/* The whole line is a commend or empty. */
		return OK;

	} else if (opt[optlen] != 0) {
		/* Part of the option name is a comment, so the value part
		 * should be ignored. */
		valuelen = 0;
		opt[optlen] = value[valuelen] = 0;
	} else {
		/* Else look for comment endings in the value. */
		valuelen = strcspn(value, "#;");
		value[valuelen] = 0;
	}

	if (set_option(opt, optlen, value, valuelen) == ERR) {
		fprintf(stderr, "Error on line %d, near '%.*s' option: %s\n",
			config_lineno, optlen, opt, config_msg);
		config_errors = TRUE;
	}

	/* Always keep going if errors are encountered. */
	return OK;
}

static int
load_options(void)
{
	char *home = getenv("HOME");
	char buf[1024];
	FILE *file;

	config_lineno = 0;
	config_errors = FALSE;

	if (!home ||
	    snprintf(buf, sizeof(buf), "%s/.tigrc", home) >= sizeof(buf))
		return ERR;

	/* It's ok that the file doesn't exist. */
	file = fopen(buf, "r");
	if (!file)
		return OK;

	if (read_properties(file, " \t", read_option) == ERR ||
	    config_errors == TRUE)
		fprintf(stderr, "Errors while loading %s.\n", buf);

	return OK;
}


 * For long loading views (taking over 3 seconds) the time since loading
 * started will be appended:
 *
 *	[main] 77d9e40fbcea3238015aea403e06f61542df9a31 - commit 1 of 779 (0%) 5s
	return read_properties(popen(cmd, "r"), "\t", read_ref);
read_repo_config_option(char *name, int namelen, char *value, int valuelen)
load_repo_config(void)
	return read_properties(popen("git repo-config --list", "r"),
			       "=", read_repo_config_option);
read_properties(FILE *pipe, const char *separators,
		char *value;
		size_t namelen;
		size_t valuelen;

		name = chomp_string(name);
		namelen = strcspn(name, separators);
		if (name[namelen]) {
			name[namelen] = 0;
			value = chomp_string(name + namelen + 1);
		state = read_property(name, namelen, value, valuelen);
	if (load_options() == ERR)
		die("Failed to load user config.");

	/* Load the repo config file so options can be overwritten from
	if (load_repo_config() == ERR)
 * include::BUGS[]
 * include::SITES[]