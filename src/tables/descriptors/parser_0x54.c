/* 
 * Copyright (c) 2008-2010, Lucas C. Villa Real <lucasvr@gobolinux.org>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. Neither the name of GoboLinux nor the names of its contributors may
 * be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "demuxfs.h"
#include "fsutils.h"
#include "xattr.h"
#include "ts.h"
#include "descriptors.h"

struct formatted_descriptor {
	uint8_t content_nibble_level_1:4;
	uint8_t content_nibble_level_2:4;
	uint8_t user_nibble:4;
	uint8_t user_nibble_unused:4;
	char *genre;
	char *subgenre;
};

static int format_genre_isdb(uint8_t genre_id, struct formatted_descriptor *f)
{
	switch (genre_id) {
		case 0x00:
			return asprintf(&f->genre, "News/Report");
		case 0x01:
			return asprintf(&f->genre, "Sports");
		case 0x02:
			return asprintf(&f->genre, "Information/Tabloid show");
		case 0x03:
			return asprintf(&f->genre, "Drama");
		case 0x04:
			return asprintf(&f->genre, "Music");
		case 0x05:
			return asprintf(&f->genre, "Variety show");
		case 0x06:
			return asprintf(&f->genre, "Movies");
		case 0x07:
			return asprintf(&f->genre, "Animation/Special effect movies");
		case 0x08:
			return asprintf(&f->genre, "Documentary/Culture");
		case 0x09:
			return asprintf(&f->genre, "Theatre/Public performance");
		case 0x0a:
			return asprintf(&f->genre, "Hobby/Education");
		case 0x0b:
			return asprintf(&f->genre, "Welfare");
		case 0x0c:
			return asprintf(&f->genre, "Reserved");
		case 0x0d:
			return asprintf(&f->genre, "Reserved");
		case 0x0e:
			return asprintf(&f->genre, "For extension");
		case 0x0f:
			return asprintf(&f->genre, "Others");
		default:
			return 0;
	}
}

static int format_genre_sbtvd(uint8_t genre_id, struct formatted_descriptor *f)
{
	switch (genre_id) {
		case 0x00:
			return asprintf(&f->genre, "Jornalismo");
		case 0x01:
			return asprintf(&f->genre, "Esporte");
		case 0x02:
			return asprintf(&f->genre, "Educativo");
		case 0x03:
			return asprintf(&f->genre, "Novela");
		case 0x04:
			return asprintf(&f->genre, "Minissérie");
		case 0x05:
			return asprintf(&f->genre, "Série/Seriado");
		case 0x06:
			return asprintf(&f->genre, "Variedade");
		case 0x07:
			return asprintf(&f->genre, "Reality show");
		case 0x08:
			return asprintf(&f->genre, "Informação");
		case 0x09:
			return asprintf(&f->genre, "Humorístico");
		case 0x0a:
			return asprintf(&f->genre, "Infantil");
		case 0x0b:
			return asprintf(&f->genre, "Erótico");
		case 0x0c:
			return asprintf(&f->genre, "Filme");
		case 0x0d:
			return asprintf(&f->genre, "Sorteio,Televendas,Premiação");
		case 0x0e:
			return asprintf(&f->genre, "Debate/Entrevista");
		case 0x0f:
			return asprintf(&f->genre, "Outros");
		default:
			return 0;
	}
}

static int format_genre_other(uint8_t genre_id, struct formatted_descriptor *f)
{
	/* TODO */
	return 0;
}

static int format_subgenre_isdb(uint8_t genre_id, uint8_t subgenre_id, struct formatted_descriptor *f)
{
	/* TODO */
	return 0;
}

static int format_subgenre_sbtvd(uint8_t genre_id, uint8_t subgenre_id, struct formatted_descriptor *f)
{
	if (genre_id == 0) {
		switch (subgenre_id) {
			case 0x00:
				return asprintf(&f->subgenre, "Telejornais");
			case 0x01:
				return asprintf(&f->subgenre, "Reportagem");
			case 0x02:
				return asprintf(&f->subgenre, "Documentário");
			case 0x03:
				return asprintf(&f->subgenre, "Biografia");
			default:
				return 0;
		}
	} else if (genre_id == 0x01) {
		switch (subgenre_id) {
			case 0x00:
				return asprintf(&f->subgenre, "Esporte");
			case 0x0f:
				return asprintf(&f->subgenre, "Outros");
			default:
				return 0;
		}
	} else if (genre_id == 0x02) {
		switch (subgenre_id) {
			case 0x00:
				return asprintf(&f->subgenre, "Educativo");
			case 0x0f:
				return asprintf(&f->subgenre, "Outros");
			default:
				return 0;
		}
	} else if (genre_id == 0x03) {
		switch (subgenre_id) {
			case 0x00:
				return asprintf(&f->subgenre, "Novela");
			case 0x0f:
				return asprintf(&f->subgenre, "Outros");
			default:
				return 0;
		}
	} else if (genre_id == 0x04) {
		switch (subgenre_id) {
			case 0x00:
				return asprintf(&f->subgenre, "Minissérie");
			case 0x0f:
				return asprintf(&f->subgenre, "Outros");
			default:
				return 0;
		}
	} else if (genre_id == 0x05) {
		switch (subgenre_id) {
			case 0x00:
				return asprintf(&f->subgenre, "Série");
			case 0x0f:
				return asprintf(&f->subgenre, "Outros");
			default:
				return 0;
		}
	} else if (genre_id == 0x06) {
		switch (subgenre_id) {
			case 0x00:
				return asprintf(&f->subgenre, "Auditório");
			case 0x01:
				return asprintf(&f->subgenre, "Show");
			case 0x02:
				return asprintf(&f->subgenre, "Musical");
			case 0x03:
				return asprintf(&f->subgenre, "Making of");
			case 0x04:
				return asprintf(&f->subgenre, "Feminino");
			case 0x05:
				return asprintf(&f->subgenre, "Game show");
			case 0x0f:
				return asprintf(&f->subgenre, "Outros");
			default:
				return 0;
		}
	} else if (genre_id == 0x07) {
		switch (subgenre_id) {
			case 0x00:
				return asprintf(&f->subgenre, "Reality show");
			case 0x0f:
				return asprintf(&f->subgenre, "Outros");
			default:
				return 0;
		}
	} else if (genre_id == 0x08) {
		switch (subgenre_id) {
			case 0x00:
				return asprintf(&f->subgenre, "Culinária");
			case 0x01:
				return asprintf(&f->subgenre, "Moda");
			case 0x02:
				return asprintf(&f->subgenre, "Rural");
			case 0x03:
				return asprintf(&f->subgenre, "Saúde");
			case 0x04:
				return asprintf(&f->subgenre, "Turismo");
			case 0x0f:
				return asprintf(&f->subgenre, "Outros");
			default:
				return 0;
		}
	} else if (genre_id == 0x09) {
		switch (subgenre_id) {
			case 0x00:
				return asprintf(&f->subgenre, "Humorístico");
			case 0x0f:
				return asprintf(&f->subgenre, "Outros");
			default:
				return 0;
		}
	} else if (genre_id == 0x0a) {
		switch (subgenre_id) {
			case 0x00:
				return asprintf(&f->subgenre, "Infantil");
			case 0x0f:
				return asprintf(&f->subgenre, "Outros");
			default:
				return 0;
		}
	} else if (genre_id == 0x0b) {
		switch (subgenre_id) {
			case 0x00:
				return asprintf(&f->subgenre, "Erótico");
			case 0x0f:
				return asprintf(&f->subgenre, "Outros");
			default:
				return 0;
		}
	} else if (genre_id == 0x0c) {
		switch (subgenre_id) {
			case 0x00:
				return asprintf(&f->subgenre, "Filme");
			case 0x0f:
				return asprintf(&f->subgenre, "Outros");
			default:
				return 0;
		}
	} else if (genre_id == 0x0d) {
		switch (subgenre_id) {
			case 0x00:
				return asprintf(&f->subgenre, "Sorteio");
			case 0x01:
				return asprintf(&f->subgenre, "Televendas");
			case 0x02:
				return asprintf(&f->subgenre, "Premiação");
			case 0x0f:
				return asprintf(&f->subgenre, "Outros");
			default:
				return 0;
		}
	} else if (genre_id == 0x0e) {
		switch (subgenre_id) {
			case 0x00:
				return asprintf(&f->subgenre, "Debate");
			case 0x01:
				return asprintf(&f->subgenre, "Entrevista");
			case 0x0f:
				return asprintf(&f->subgenre, "Outros");
			default:
				return 0;
		}
	} else if (genre_id == 0x0f) {
		switch (subgenre_id) {
			case 0x00:
				return asprintf(&f->subgenre, "Desenho adulto");
			case 0x01:
				return asprintf(&f->subgenre, "Interativo");
			case 0x02:
				return asprintf(&f->subgenre, "Político");
			case 0x03:
				return asprintf(&f->subgenre, "Religioso");
			case 0x0f:
				return asprintf(&f->subgenre, "Outros");
			default:
				return 0;
		}
	}
	return 0;
}

static int format_subgenre_other(uint8_t genre_id, uint8_t subgenre_id, struct formatted_descriptor *f)
{
	/* TODO */
	return 0;
}

/* CONTENT_DESCRIPTOR parser */
int descriptor_0x54_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	int i = 2, index = 0;

	if (! descriptor_is_parseable(parent, payload[0], 4, len))
		return -ENODATA;

	while (len) {
		struct dentry *dentry = CREATE_DIRECTORY(parent, "Content_Descriptor_%02d", index++);
		struct formatted_descriptor f;

		f.content_nibble_level_1 = (payload[i] >> 4) & 0x0f;
		f.content_nibble_level_2 = payload[i] & 0x0f;
		f.user_nibble = (payload[i+1] >> 4) & 0x0f;
		f.user_nibble_unused = payload[i+1] & 0x0f;
		i += 2;
		len -= 2;
		
		if (priv->options.standard == ISDB_STANDARD) {
			format_genre_isdb(f.content_nibble_level_1, &f);
			format_subgenre_isdb(f.content_nibble_level_1, f.content_nibble_level_2, &f);
		} else if (priv->options.standard == SBTVD_STANDARD) {
			format_genre_sbtvd(f.content_nibble_level_1, &f);
			format_subgenre_sbtvd(f.content_nibble_level_1, f.content_nibble_level_2, &f);
		} else {
			format_genre_other(f.content_nibble_level_1, &f);
			format_subgenre_other(f.content_nibble_level_1, f.content_nibble_level_2, &f);
		}

		/* XXX well.. we're not merging the number with the genre string as we usually do */
		CREATE_FILE_NUMBER(dentry, &f, content_nibble_level_1);
		CREATE_FILE_NUMBER(dentry, &f, content_nibble_level_2);
		CREATE_FILE_NUMBER(dentry, &f, user_nibble);
		if (f.genre) {
			CREATE_FILE_STRING(dentry, &f, genre, XATTR_FORMAT_STRING);
			free(f.genre);
		}
		if (f.subgenre) {
			CREATE_FILE_STRING(dentry, &f, subgenre, XATTR_FORMAT_STRING);
			free(f.subgenre);
		}
	}
    return 0;
}

