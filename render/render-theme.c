//
// Copyright (C) 2025-2026 Free Software Foundation, Inc.

// Author: Divya Ranjan Pattanaik <divya@dialetheia.xyz>

//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "render-theme.h"
#include "elisp-helpers.h"
#include "render-core.h"
#include "render-threads.h"

/**
 * Toggle dark theme rendering for the current document page.
 *
 * Inverts the `invert` flag in the DocState, re-renders the current
 * page in a background thread, waits for it to finish, and updates
 * the Emacs overlay with the new image.
 *
 * @param env    Emacs module environment.
 * @param nargs  Argument count (unused).
 * @param args   Argument values (unused).
 * @param data   Additional data (unused).
 * Return:       EMACS_T on success, EMACS_NIL if no DocState is available.
 */

emacs_value
emacs_doc_invert(emacs_env *env, ptrdiff_t nargs, emacs_value *args,
                     void *data)
{
	(void)nargs;
	(void)data;

	DocState *doc_state = get_doc_state_ptr(env);

	if (!doc_state)
		{
			emacs_message(env, "No valid DocState user pointer found.");
			return EMACS_NIL;
		}

	int invert = env->extract_integer(env, args[0]);

	doc_state->invert = invert ? 1 : 0;

	return EMACS_T;
}

/**
 * Map black to BLACK and white to WHITE, 0xRRGGBB integers, in pages
 * rendered from now on; nil for either turns the mapping off.
 */

void
set_doc_tint(emacs_env *env, DocState *doc_state, emacs_value black,
	     emacs_value white)
{
	if (env->is_not_nil(env, black) && env->is_not_nil(env, white))
		{
			doc_state->tint_black
				= env->extract_integer(env, black);
			doc_state->tint_white
				= env->extract_integer(env, white);
			doc_state->tint = 1;
		}
	else
		doc_state->tint = 0;
}

/** Module entry for `set_doc_tint' on the current document. */

emacs_value
emacs_doc_tint(emacs_env *env, ptrdiff_t nargs, emacs_value *args,
	       void *data)
{
	(void)nargs;
	(void)data;

	DocState *doc_state = get_doc_state_ptr(env);

	if (!doc_state)
		{
			emacs_message(env,
				      "No valid DocState user pointer found.");
			return EMACS_NIL;
		}

	set_doc_tint(env, doc_state, args[0], args[1]);

	return EMACS_T;
}

static void
tint_sample(unsigned char *s, int lift[3][256])
{
	int l = (77 * s[0] + 150 * s[1] + 29 * s[2] + 128) >> 8;

	for (int c = 0; c < 3; c++)
		{
			int v = lift[c][l] + s[c];

			s[c] = v < 0 ? 0 : v > 255 ? 255 : v;
		}
}

static void
tint_row(unsigned char *s, int w, int n, int lift[3][256],
	 unsigned char *seen, unsigned char *made)
{
	for (int x = 0; x < w; x++, s += n)
		{
			if (s[0] == seen[0] && s[1] == seen[1]
			    && s[2] == seen[2])
				{
					s[0] = made[0];
					s[1] = made[1];
					s[2] = made[2];
					continue;
				}
			seen[0] = s[0];
			seen[1] = s[1];
			seen[2] = s[2];
			tint_sample(s, lift);
			made[0] = s[0];
			made[1] = s[1];
			made[2] = s[2];
		}
}

/**
 * Map black to BLACK and white to WHITE by lightness, keeping each pixel's
 * offset from gray: fz_tint_pixmap flips hues when BLACK is the lighter one.
 */

void
tint_pixmap_keep_hue(fz_context *ctx, fz_pixmap *pixmap, int black, int white)
{
	int lift[3][256]; // channel c of the gray at lightness l, minus l
	int w = fz_pixmap_width(ctx, pixmap);
	int h = fz_pixmap_height(ctx, pixmap);
	int n = fz_pixmap_components(ctx, pixmap);
	int stride = fz_pixmap_stride(ctx, pixmap);
	unsigned char *samples = fz_pixmap_samples(ctx, pixmap);

	for (int c = 0; c < 3; c++)
		{
			int from = (black >> (16 - 8 * c)) & 255;
			int to = (white >> (16 - 8 * c)) & 255;

			for (int l = 0; l < 256; l++)
				lift[c][l] = from + l * (to - from) / 255 - l;
		}

	// Pages are mostly paper, so most pixels repeat the previous one.
	unsigned char seen[3] = { 255, 255, 255 };
	unsigned char made[3] = { 255, 255, 255 };
	tint_sample(made, lift);

	for (int y = 0; y < h; y++, samples += stride)
		tint_row(samples, w, n, lift, seen, made);
}
