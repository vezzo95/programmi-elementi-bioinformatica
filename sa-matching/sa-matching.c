/*
  Copyright (C) 2014 by Gianluca Della Vedova


  You can redistribute this file and/or modify it
  under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Box is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this file
  If not, see <http://www.gnu.org/licenses/>.
*/

#include "sa-matching.h"

KSEQ_INIT(gzFile, gzread)

static inline int min(int a, int b) {
        return a < b ? a : b;
}

static inline int mid(int a, int b) {
        return a + (b - a) / 2;
}

/* compares a pattern (a string) and a suffix of the text, by extracting the prefix of
   the text with same length as the pattern. The return value is:
   = 0 if the pattern is equal the prefix
   < 0 if the pattern precedes the prefix (in lexicographical order)
   > 0 if the pattern follows  the prefix (in lexicographical order)
*/
static int
suffix_cmp(const char* text, const unsigned int pos, const unsigned int n,
           const char* pattern, const unsigned int m) {
        size_t size = min(m , n - pos + 1);
        return strncmp(pattern, text + pos, size);
}


static char* read_text(char* filename) {
        gzFile fp;
        kseq_t *seq;
        fp = gzopen(filename, "r");
        assert(fp != NULL && "Could not open fasta file\n");
        seq = kseq_init(fp);
        int res = kseq_read(seq);
        assert(res >= 0);
        gzclose(fp);
        char* text = NULL;
        int err = asprintf(&text, "%s", seq->seq.s);
        assert(err >= 0);
        kseq_destroy(seq);
        return text;
}

/* returns the position of the suffix array corresponding to a match,
   returns -1 if no such match exists
*/

static unsigned int*
lcp_kasai(const char* text, const int* sa, const unsigned int n) {
        //lcp-array allocation
        unsigned int* lcp = (unsigned int*) malloc(n * sizeof(unsigned int));
        if(lcp == NULL)
                return lcp;

        //inverted-suffix-array allocation
        unsigned int* i_sa = (unsigned int*) malloc(n * sizeof(unsigned int));
        if(i_sa == NULL) {
                free(lcp);
                return NULL;
        }
        
        //inverted suffix array filling: 
        //Memorize, for each i-suffix of the string, his rank in suffix array
        for(unsigned int i = 0; i < n; i++)
                i_sa[sa[i]] = i;
        
        unsigned int l = 0;
        unsigned int k, j;
        for(unsigned int i = 0; i < n - 1; i++) {
                k = i_sa[i];    
                j = sa[k - 1];
                /* 
                i = sa[k]
                j = sa[k - 1]
                */
                while(text[i + l] == text[j + l])
                        l++;
                lcp[k] = l;
                if(l > 0)
                        l = l - 1;
        }
        //lcp[0] is always equals to 0
        lcp[0] = 0;
        
        //inverted-suffix-array memory cleaning
        free(i_sa);
        return lcp;
}

static unsigned int
sa_search(const char* text, const int* sa, const unsigned int n,
          const unsigned int* lcp, const unsigned int* lcplr, const unsigned int o, const char* pattern, const unsigned int m) {
        unsigned int found = -1;
        if (suffix_cmp(text, sa[0], n, pattern, m) < 0 ||
            suffix_cmp(text, sa[n - 1], n, pattern, m) > 0)
                return -1;
        /* The pattern is not outside the realm of available suffixes.
           Let's search for an occurrence
        */
        unsigned int lp = 0;
        unsigned int rp = n - 1;
        while (lp <= rp) {
                unsigned int mp = lp + (rp - lp) / 2;
                int cmp = suffix_cmp(text, sa[mp], n, pattern, m);
                if (cmp == 0) {
                        found = mp;
                        break;
                }
                if (cmp < 0)
                        rp = mp - 1;
                else
                        lp = mp + 1;
        }
        return found;
}

/* Accelerant #1 */
static unsigned int
lcp2(const char* s1, const char* s2, const unsigned int len) {
        unsigned int l = 0;
        for (;l < len && s1[l] == s2[l]; l++) {}
        return l;
}

static unsigned int
sa_search_1(const char* text, const int* sa, const unsigned int n,
          const unsigned int* lcp, const unsigned int* lcplr, const unsigned int o, const char* pattern, const unsigned int m) {
        unsigned int found = -1;
        if (suffix_cmp(text, sa[0], n, pattern, m) < 0 ||
            suffix_cmp(text, sa[n - 1], n, pattern, m) > 0)
                return -1;
        /* The pattern is not outside the realm of available suffixes.
           Let's search for an occurrence
        */
        unsigned int lp = 0;
        unsigned int rp = n - 1;
        unsigned int l = lcp2(text + sa[lp], pattern, m);
        unsigned int r = lcp2(text + sa[rp], pattern, m);
        unsigned int mlr = min(l, r);
        while (lp <= rp) {
                unsigned int mp = lp + (rp - lp) / 2;
                int cmp = suffix_cmp(text, sa[mp] + mlr, n - mlr, pattern + mlr, m - mlr);
                if (cmp == 0) {
                        found = mp;
                        break;
                }
                if (cmp < 0) {
                        rp = mp - 1;
                        r = mlr + lcp2(text + sa[rp] +mlr, pattern + mlr, min(m - mlr, n - mlr));
                } else {
                        lp = mp + 1;
                        l = mlr + lcp2(text + sa[lp] +mlr, pattern + mlr, min(m - mlr, n - mlr));
                }
                mlr = min(l, r);
        }
        return found;
}

/* Accelerant #2 */
static unsigned int
sa_search_2(const char* text, const int* sa, const unsigned int n,
          const unsigned int* lcp, const unsigned int* lcplr, const unsigned int o, const char* pattern, const unsigned int m) {
        unsigned int found = -1;
        if (suffix_cmp(text, sa[0], n, pattern, m) < 0 ||
            suffix_cmp(text, sa[n - 1], n, pattern, m) > 0)
                return -1;
        /* The pattern is not outside the realm of available suffixes.
           Let's search for an occurrence
        */
        unsigned int lp = 0;
        unsigned int rp = n - 1;
        unsigned int l = lcp2(text + sa[lp], pattern, m);
        unsigned int r = lcp2(text + sa[rp], pattern, m);
        unsigned int
                lcp_t(const unsigned int p1, const unsigned int p2) {
                return lcp2(text + sa[p1], text + sa[p2], m);
        }

        while (lp < rp - 1) {
                unsigned int mp = lp + (rp - lp) / 2;
                if (l > r)
                        if (lcp_t(lp, mp) > l) {
                                lp = mp;
                        } else if (lcp_t(lp, mp) < l) {
                                rp = mp;
                                r += lcp2(text + sa[rp] + r, pattern + r, min(m - r, n - r));
                        } else {
                                /* lcp_t(lp, mp) == l */
                                int cmp = suffix_cmp(text, sa[mp] + l, n - l, pattern + l, m - l);
                                if (cmp == 0) {
                                        found = mp;
                                        break;
                                }
                                if (cmp < 0) {
                                        rp = mp;
                                        r += lcp2(text + sa[rp] + l, pattern + l, min(m - l, n - l));
                                } else {
                                        lp = mp;
                                        l += lcp2(text + sa[lp] + l, pattern + l, min(m - l, n - l));
                                }
                        }
                else if (l < r)
                        if (lcp_t(rp, mp) > r) {
                                rp = mp;
                        } else if (lcp_t(rp, mp) < r) {
                                lp = mp;
                                l += lcp2(text + sa[lp] + r, pattern + l, min(m - l, n - l));
                        } else {
                                /* lcp_t(rp, lp) == r */
                                int cmp = suffix_cmp(text, sa[mp] + r, n - r, pattern + r, m - r);
                                if (cmp == 0) {
                                        found = mp;
                                        break;
                                }
                                if (cmp < 0) {
                                        lp = mp;
                                        l += lcp2(text + sa[lp] + r, pattern + r, min(m - r, n - r));
                                } else {
                                        lp = mp;
                                        r += lcp2(text + sa[rp] + r, pattern + r, min(m - r, n - r));
                                }
                        }
                else {
                        /* l == r */
                        if (lcp_t(lp, rp) > l) {
                                lp = mp;
                        } else if (lcp_t(rp, lp) > r) {
                                rp = mp;
                        } else {
                                /* lcp_t(mp, rp) == lcp_t(lp, mp) == l */
                                int cmp = suffix_cmp(text, sa[mp] + l, n - l, pattern + l, m - l);
                                if (cmp == 0) {
                                        found = mp;
                                        break;
                                }
                                if (cmp < 0) {
                                        rp = mp;
                                        r += lcp2(text + sa[rp] + l, pattern + l, min(m - l, n - l));
                                } else {
                                        lp = mp;
                                        l += lcp2(text + sa[lp] + l, pattern + l, min(m - l, n - l));
                                }
                        }
                }
        }
        if (found == -1) {
                if (suffix_cmp(text, sa[lp], n, pattern, m) == 0) found = lp;
                if (suffix_cmp(text, sa[rp], n, pattern, m) == 0) found = rp;
        }
        return found;
}

/* Accelerant #3 */
static unsigned int
lcplr_compute(const unsigned int* lcp, const unsigned int ss, const unsigned int se,
              unsigned int* lcplr, const unsigned int si) {
        /* lcplr is a segment tree
           ss is a segment start
           se is a segment end
           si is the lcplr memory cell that I want compute */
        if(ss == se) {
                lcplr[si] = lcp[ss];
                return lcplr[si];
        }
                
        int m = mid(ss, se);
        lcplr[si] = min(lcplr_compute(lcp, ss, m, lcplr, 2 * si + 1),
                        lcplr_compute(lcp, m + 1, se, lcplr, 2 * si + 2));
        return lcplr[si];        
}

static unsigned int*
lcplr_build(const unsigned int* lcp, const unsigned int n, const unsigned int o) {
        unsigned int* lcplr = (unsigned int*) malloc(o * sizeof(unsigned int));
        lcplr_compute(lcp, 0, n-1, lcplr, 0);
        return lcplr;
}

static unsigned int
rmq_utils(const unsigned int* lcplr, const unsigned int ss, const unsigned int se, 
    const unsigned int qs, const unsigned int qe, const unsigned int index) {
        /* lcplr is a segment tree
           ss is a segment start
           se is a segment end
           qs is the query interval start
           qe is the query interval end
           index is the lcplr memory cell that I'm currently watching in */
        
        //if segment ss to se is part of query interval
        if(qs <= ss && qe >= se)
                return lcplr[index];
        
        //if segment ss to se is outside query interval
        if(se < qs || ss > qe)
                return UINT_MAX;
        
        //if part of the segment overlap with query interval
        unsigned int m = mid(ss, se);
        return min(rmq_utils(lcplr, ss, m, qs, qe, 2 * index + 1),
                   rmq_utils(lcplr, m + 1, se, qs, qe, 2 * index + 2));
}

static unsigned int
rmq(const unsigned int* lcplr, const unsigned int n, const unsigned int qs, const unsigned int qe) {
        if(qs < 0 || qe > n - 1 || qs > qe)
                return 0;
        return rmq_utils(lcplr, 0, n-1, qs, qe, 0);
}

static unsigned int
sa_search_3(const char* text, const int* sa, const unsigned int n,
          const unsigned int* lcp, const unsigned int* lcplr, const unsigned int o, const char* pattern, const unsigned int m) {
        unsigned int found = -1;
        if (suffix_cmp(text, sa[0], n, pattern, m) < 0 ||
            suffix_cmp(text, sa[n - 1], n, pattern, m) > 0)
                return -1;
        /* The pattern is not outside the realm of available suffixes.
           Let's search for an occurrence
        */
        unsigned int lp = 0;
        unsigned int rp = n - 1;
        unsigned int l = lcp2(text + sa[lp], pattern, m);
        unsigned int r = lcp2(text + sa[rp], pattern, m);

        while (lp < rp - 1) {
                unsigned int mp = lp + (rp - lp) / 2;
                if (l > r)
                        if (rmq(lcplr, n, lp, mp) > l) {
                                lp = mp;
                        } else if (rmq(lcplr, n, lp, mp) < l) {
                                rp = mp;
                                r += lcp2(text + sa[rp] + r, pattern + r, min(m - r, n - r));
                        } else {
                                /* lcp_t(lp, mp) == l */
                                int cmp = suffix_cmp(text, sa[mp] + l, n - l, pattern + l, m - l);
                                if (cmp == 0) {
                                        found = mp;
                                        break;
                                }
                                if (cmp < 0) {
                                        rp = mp;
                                        r += lcp2(text + sa[rp] + l, pattern + l, min(m - l, n - l));
                                } else {
                                        lp = mp;
                                        l += lcp2(text + sa[lp] + l, pattern + l, min(m - l, n - l));
                                }
                        }
                else if (l < r)
                        if (rmq(lcplr, n, rp, mp) > r) {
                                rp = mp;
                        } else if (rmq(lcplr, n, rp, mp) < r) {
                                lp = mp;
                                l += lcp2(text + sa[lp] + r, pattern + l, min(m - l, n - l));
                        } else {
                                /* lcp_t(rp, lp) == r */
                                int cmp = suffix_cmp(text, sa[mp] + r, n - r, pattern + r, m - r);
                                if (cmp == 0) {
                                        found = mp;
                                        break;
                                }
                                if (cmp < 0) {
                                        lp = mp;
                                        l += lcp2(text + sa[lp] + r, pattern + r, min(m - r, n - r));
                                } else {
                                        lp = mp;
                                        r += lcp2(text + sa[rp] + r, pattern + r, min(m - r, n - r));
                                }
                        }
                else {
                        /* l == r */
                        if (rmq(lcplr, n, lp, rp) > l) {
                                lp = mp;
                        } else if (rmq(lcplr, n, rp, lp) > r) {
                                rp = mp;
                        } else {
                                /* lcp_t(mp, rp) == lcp_t(lp, mp) == l */
                                int cmp = suffix_cmp(text, sa[mp] + l, n - l, pattern + l, m - l);
                                if (cmp == 0) {
                                        found = mp;
                                        break;
                                }
                                if (cmp < 0) {
                                        rp = mp;
                                        r += lcp2(text + sa[rp] + l, pattern + l, min(m - l, n - l));
                                } else {
                                        lp = mp;
                                        l += lcp2(text + sa[lp] + l, pattern + l, min(m - l, n - l));
                                }
                        }
                }
        }
        if (found == -1) {
                if (suffix_cmp(text, sa[lp], n, pattern, m) == 0) found = lp;
                if (suffix_cmp(text, sa[rp], n, pattern, m) == 0) found = rp;
        }
        return found;
}

int main(int argc, char **argv) {
        static struct gengetopt_args_info args_info;
        assert(cmdline_parser(argc, argv, &args_info) == 0);
        char* pattern = args_info.pattern_orig;
        char* text = read_text(args_info.text_arg);
        uint32_t n = strlen(text);
        uint32_t m = strlen(pattern);

        unsigned int (*search_f)(const char*, const int*, const unsigned int, const unsigned int*,
                                 const unsigned int*, const unsigned int, const char*, const unsigned int);

        printf("Accelerant type %d\n", args_info.accelerant_arg);
        if (args_info.accelerant_arg == 1)
                search_f = sa_search_1;
        else if (args_info.accelerant_arg == 2)
                search_f = sa_search_2;
        else if (args_info.accelerant_arg == 3)
                search_f = sa_search_3;
        else
                search_f = sa_search; 



        int* sa = malloc(n * sizeof(*sa));
        //unsigned int* lcp = malloc(n * sizeof(*sa));
        if(sais((unsigned char*) text, sa, (int)n) != 0) {
                fprintf(stderr, "%s: Cannot allocate memory.\n", argv[0]);
                exit(EXIT_FAILURE);
        }
        //sais_lcp((unsigned char*) text, sa, lcp, n);
        //lcp[0] = 0;
        unsigned int* lcp = lcp_kasai(text, sa, n);
        unsigned int o = 2 * pow(2, log2(n)) - 1;
        unsigned int* lcplr = lcplr_build(lcp, n, o);
        unsigned int found = search_f(text, sa, n, lcp, lcplr, o, pattern, m);

        /* If found != -1 we have found at least one occurrence.
           Let's find all of them!
        */
        if (found != -1) {
                unsigned int lp = found;
                for (;lcp[lp] >= m; lp--) {}
                unsigned int rp = found + 1;
                for (;lcp[rp] >= m; rp++) {}
                for (unsigned int pos = lp; pos < rp; pos++) {
                        char* x = strndupa(text + sa[pos], m);
                        printf("Occurrence %s at position %d\n", x, sa[pos]);
                }
        }
        free(sa);
        free(lcp);
        free(text);
}
