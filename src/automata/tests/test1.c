#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <CUnit/Basic.h>

#include "automata.h"

void testBack(void)
{
    AUTOMATA a = automata_ascii_new(FALSE);
    CU_ASSERT(automata_ascii_add_backwards(a, "example.com", 0));
    CU_ASSERT(automata_ascii_add_backwards(a, "*.example.com", 0));
    CU_ASSERT(automata_ascii_add_backwards(a, "example.net", 0));
    CU_ASSERT(automata_ascii_add_backwards(a, "*.example.net", 0));
    char *buf = automata_ascii_compile(a, NULL, NULL);
    automata_ascii_free(a);
    CU_ASSERT_PTR_NOT_NULL_FATAL(buf);

    CU_ASSERT(automata_ascii_backwards_match(buf, "example.com"));
    CU_ASSERT(! automata_ascii_backwards_match(buf, "exmaple.com"));
    CU_ASSERT(! automata_ascii_backwards_match(buf, "test.example.com"));
    CU_ASSERT(! automata_ascii_backwards_match(buf, "asdfasdf.example.com"));
    CU_ASSERT(! automata_ascii_backwards_match(buf, "example.org"));
    CU_ASSERT(automata_ascii_backwards_match(buf, "example.net"));
    CU_ASSERT(! automata_ascii_backwards_match(buf, "example.net.ua"));
    CU_ASSERT(! automata_ascii_backwards_match(buf, "bla.example.net"));
    CU_ASSERT(! automata_ascii_backwards_match(buf, "www.example.net"));
    CU_ASSERT(! automata_ascii_backwards_match(buf, "myexample.net"));

    free(buf);
}

void testBackStarSelect(void)
{
    AUTOMATA a = automata_ascii_new(TRUE);
    CU_ASSERT(automata_ascii_add_backwards_star(a, "example.com", 1));
    CU_ASSERT(automata_ascii_add_backwards_star(a, "*.example.com", 1));
    CU_ASSERT(automata_ascii_add_backwards_star(a, "example.net", 2));
    CU_ASSERT(automata_ascii_add_backwards_star(a, "*.example.net", 2));
    char *buf = automata_ascii_compile(a, NULL, NULL);
    automata_ascii_free(a);
    CU_ASSERT_PTR_NOT_NULL_FATAL(buf);

    CU_ASSERT(automata_ascii_backwards_match(buf, "example.com"));
    CU_ASSERT(automata_ascii_backwards_match(buf, "test.example.com"));
    CU_ASSERT(automata_ascii_backwards_match(buf, "asdfasdf.example.com"));
    CU_ASSERT(! automata_ascii_backwards_match(buf, "example.org"));
    CU_ASSERT(automata_ascii_backwards_match(buf, "example.net"));
    CU_ASSERT(! automata_ascii_backwards_match(buf, "example.net.ua"));
    CU_ASSERT(automata_ascii_backwards_match(buf, "bla.example.net"));
    CU_ASSERT(automata_ascii_backwards_match(buf, "www.example.net"));
    CU_ASSERT(! automata_ascii_backwards_match(buf, "myexample.net"));

    CU_ASSERT_EQUAL(automata_ascii_backwards_select(buf, "example.com"), 1);
    CU_ASSERT_EQUAL(automata_ascii_backwards_select(buf, "test.example.com"), 1);
    CU_ASSERT_EQUAL(automata_ascii_backwards_select(buf, "asdfasdf.example.com"), 1);
    CU_ASSERT_EQUAL(automata_ascii_backwards_select(buf, "example.org"), 0);
    CU_ASSERT_EQUAL(automata_ascii_backwards_select(buf, "example.net"), 2);
    CU_ASSERT_EQUAL(automata_ascii_backwards_select(buf, "example.net.ua"), 0);
    CU_ASSERT_EQUAL(automata_ascii_backwards_select(buf, "bla.example.net"), 2);
    CU_ASSERT_EQUAL(automata_ascii_backwards_select(buf, "www.example.net"), 2);
    CU_ASSERT_EQUAL(automata_ascii_backwards_select(buf, "myexample.net"), 0);

    free(buf);
}

void testForwardSelect(void)
{
    AUTOMATA a = automata_ascii_new(TRUE);
    CU_ASSERT(automata_ascii_add_forwards_star(a, "/images/*", 1));
    CU_ASSERT(automata_ascii_add_forwards_star(a, "/pages/*", 2));
    CU_ASSERT(automata_ascii_add_forwards(a, "/index.html", 2));
    CU_ASSERT(automata_ascii_add_forwards_star(a, "/files/*", 3));
    char *buf = automata_ascii_compile(a, NULL, NULL);
    automata_ascii_free(a);
    CU_ASSERT_PTR_NOT_NULL_FATAL(buf);

    CU_ASSERT_EQUAL(automata_ascii_forwards_select(buf, "/images/logo.gif"), 1);
    CU_ASSERT_EQUAL(automata_ascii_forwards_select(buf, "/index.html"), 2);
    CU_ASSERT_EQUAL(automata_ascii_forwards_select(buf, "/filestest"), 0);
    CU_ASSERT_EQUAL(automata_ascii_forwards_select(buf, "/pages/index.html"), 2);
    CU_ASSERT_EQUAL(automata_ascii_forwards_select(buf, "/files/index.html"), 3);
    CU_ASSERT_EQUAL(automata_ascii_forwards_select(buf, "/css/main.css"), 0);

    CU_ASSERT_FALSE(automata_ascii_forwards_match(buf, "/filestest"));
    CU_ASSERT_TRUE(automata_ascii_forwards_match(buf, "/index.html"));
    CU_ASSERT_TRUE(automata_ascii_forwards_match(buf, "/files/index.html"));

    free(buf);
}

void testEmptyFw(void)
{
    AUTOMATA a = automata_ascii_new(TRUE);
    CU_ASSERT(automata_ascii_add_forwards(a, "", 1));
    CU_ASSERT(automata_ascii_add_forwards(a, "abc", 2));
    char *buf = automata_ascii_compile(a, NULL, NULL);
    automata_ascii_free(a);
    CU_ASSERT_PTR_NOT_NULL_FATAL(buf);

    CU_ASSERT_EQUAL(automata_ascii_forwards_select(buf, ""), 1);
    CU_ASSERT_EQUAL(automata_ascii_forwards_select(buf, "asdf"), 0);
    CU_ASSERT_EQUAL(automata_ascii_forwards_select(buf, "abc"), 2);
    CU_ASSERT_TRUE(automata_ascii_forwards_match(buf, ""));
    CU_ASSERT_FALSE(automata_ascii_forwards_match(buf, "test"));

    CU_ASSERT_EQUAL(automata_ascii_backwards_select(buf, ""), 1);
    CU_ASSERT_EQUAL(automata_ascii_backwards_select(buf, "asdf"), 0);
    CU_ASSERT_EQUAL(automata_ascii_backwards_select(buf, "cba"), 2);
    CU_ASSERT_TRUE(automata_ascii_backwards_match(buf, ""));
    CU_ASSERT_FALSE(automata_ascii_backwards_match(buf, "test"));

    free(buf);
}
void testEmptyBk(void)
{
    AUTOMATA a = automata_ascii_new(TRUE);
    CU_ASSERT(automata_ascii_add_backwards(a, "", 1));
    CU_ASSERT(automata_ascii_add_backwards(a, "abc", 2));
    char *buf = automata_ascii_compile(a, NULL, NULL);
    automata_ascii_free(a);
    CU_ASSERT_PTR_NOT_NULL_FATAL(buf);

    CU_ASSERT_EQUAL(automata_ascii_forwards_select(buf, ""), 1);
    CU_ASSERT_EQUAL(automata_ascii_forwards_select(buf, "asdf"), 0);
    CU_ASSERT_EQUAL(automata_ascii_forwards_select(buf, "cba"), 2);
    CU_ASSERT_TRUE(automata_ascii_forwards_match(buf, ""));
    CU_ASSERT_FALSE(automata_ascii_forwards_match(buf, "test"));

    CU_ASSERT_EQUAL(automata_ascii_backwards_select(buf, ""), 1);
    CU_ASSERT_EQUAL(automata_ascii_backwards_select(buf, "asdf"), 0);
    CU_ASSERT_EQUAL(automata_ascii_backwards_select(buf, "abc"), 2);
    CU_ASSERT_TRUE(automata_ascii_backwards_match(buf, ""));
    CU_ASSERT_FALSE(automata_ascii_backwards_match(buf, "test"));

    free(buf);
}

void testBigNumbers(void)
{
    size_t A = 1000000000;
    AUTOMATA a = automata_ascii_new(TRUE);
    CU_ASSERT(automata_ascii_add_forwards_star(a, "/images/*", A+1));
    CU_ASSERT(automata_ascii_add_forwards_star(a, "/pages/*", A+2));
    CU_ASSERT(automata_ascii_add_forwards(a, "/index.html", A+2));
    CU_ASSERT(automata_ascii_add_forwards_star(a, "/files/*", (A+3)*(A+3)));
    char *buf = automata_ascii_compile(a, NULL, NULL);
    automata_ascii_free(a);
    CU_ASSERT_PTR_NOT_NULL_FATAL(buf);

    CU_ASSERT_EQUAL(automata_ascii_forwards_select(buf, "/images/logo.gif"), A+1);
    CU_ASSERT_EQUAL(automata_ascii_forwards_select(buf, "/index.html"), A+2);
    CU_ASSERT_EQUAL(automata_ascii_forwards_select(buf, "/filestest"), 0);
    CU_ASSERT_EQUAL(automata_ascii_forwards_select(buf, "/pages/index.html"), A+2);
    CU_ASSERT_TRUE(automata_ascii_forwards_select(buf, "/files/index.html") == (A+3)*(A+3));
    CU_ASSERT_EQUAL(automata_ascii_forwards_select(buf, "/css/main.css"), 0);

    CU_ASSERT_FALSE(automata_ascii_forwards_match(buf, "/filestest"));
    CU_ASSERT_TRUE(automata_ascii_forwards_match(buf, "/index.html"));
    CU_ASSERT_TRUE(automata_ascii_forwards_match(buf, "/files/index.html"));

    free(buf);
}

void testBigAutomata(void)
{
    char *fruits[] = {
        "apple",
        "orange",
        "cherry",
        "rambutan",
        "olive",
        "avocado",
        "pear",
        "banana",
        "plum",
        "apricot",
        "pineapple",
        "ananas",
        "coconut",
        "persimmon",
        "mandarin",
        "grapes",
        "watermelon",
        "muskmelon",
        "lime",
        "lemon",
        NULL};
    AUTOMATA a = automata_ascii_new(TRUE);
    char wordbuf[1024];
    for(char **w1=fruits; *w1; ++w1) {
        for(char **w2=fruits; *w2; ++w2) {
            for(char **w3=fruits; *w3; ++w3) {
                sprintf(wordbuf, "%s %s %s", *w1, *w2, *w3);
                CU_ASSERT_TRUE(automata_ascii_add_forwards(a, wordbuf,
                    (w1-fruits+1)*(w2-fruits+1)*(w3-fruits+1)));
            }
        }
    }
    char *buf = automata_ascii_compile(a, NULL, NULL);
    automata_ascii_free(a);
    CU_ASSERT_PTR_NOT_NULL_FATAL(buf);

    CU_ASSERT_EQUAL(automata_ascii_forwards_select(buf, "apple apple apple"), 1);
    CU_ASSERT(automata_ascii_forwards_match(buf, "orange orange orange"));
    CU_ASSERT_EQUAL(automata_ascii_forwards_select(buf, "apple orange cherry"), 6);
    CU_ASSERT_EQUAL(automata_ascii_forwards_select(buf, "orange cherry apple"), 6);
    CU_ASSERT_EQUAL(automata_ascii_forwards_select(buf, "potato potato potato"), 0);
    CU_ASSERT_EQUAL(automata_ascii_forwards_select(buf, "apple potato cherry"), 0);

    free(buf);
}

int main()
{
   CU_pSuite pSuite = NULL;

   /* initialize the CUnit test registry */
   if (CUE_SUCCESS != CU_initialize_registry())
      return CU_get_error();

   /* add a suite to the registry */
   pSuite = CU_add_suite("Suite_1", NULL, NULL);
   if (NULL == pSuite) {
      CU_cleanup_registry();
      return CU_get_error();
   }

   /* add the tests to the suite */
   if ((NULL == CU_add_test(pSuite, "test of exact domain match", testBack))
       || (NULL == CU_add_test(pSuite, "test of prefix domain match & select", testBackStarSelect))
       || (NULL == CU_add_test(pSuite, "test of url (path) match & select", testForwardSelect))
       || (NULL == CU_add_test(pSuite, "test of empty match (forwards)", testEmptyFw))
       || (NULL == CU_add_test(pSuite, "test of empty match (backwards)", testEmptyBk))
       || (NULL == CU_add_test(pSuite, "test big numbers", testBigNumbers))
       || (NULL == CU_add_test(pSuite, "test big automata", testBigAutomata))
       || 0) {
      CU_cleanup_registry();
      return CU_get_error();
   }

   /* Run all tests using the CUnit Basic interface */
   CU_basic_set_mode(CU_BRM_VERBOSE);
   CU_basic_run_tests();
   CU_cleanup_registry();
   return CU_get_error();
}
