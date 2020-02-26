#include <gtest/gtest.h>

#include <MICmdArgValString.h>

TEST(TestMICmdArgValString, HandlesQuotedStringWithAnySpecialContent) {
  for (uint8_t flags = 0; flags < 1 << 4; flags++) {
    CMICmdArgValString arg("arg", flags & 1, flags & 2, true, flags & 4,
                           flags & 8);

    CMICmdArgContext context("\"a/b\" \"i1\" \"10\" \"--option\"");

    EXPECT_TRUE(arg.Validate(context));
    EXPECT_EQ("a/b", arg.GetValue());

    EXPECT_TRUE(arg.Validate(context));
    EXPECT_EQ("i1", arg.GetValue());

    EXPECT_TRUE(arg.Validate(context));
    EXPECT_EQ("10", arg.GetValue());

    EXPECT_TRUE(arg.Validate(context));
    EXPECT_EQ("--option", arg.GetValue());
  }
}

TEST(TestMICmdArgValString, HandlesQuotedStringsWithQuotes) {
  CMICmdArgValString arg("arg", true, true, true, false, false);

  CMICmdArgContext context("\"a \"b\" c\"");
  EXPECT_TRUE(arg.Validate(context));
}

TEST(TestMICmdArgValString, HandlesEmbeddedQuotedStringsWithSlashes) {
  CMICmdArgValString arg("arg", true, true, true, false, false);

  CMICmdArgContext context("\"a \\\"b c\\\" d\"");
  EXPECT_TRUE(arg.Validate(context));
}

TEST(TestMICmdArgValString, DoesNotHandleSlashedQuotedStrings) {
  CMICmdArgValString arg("arg", true, true, true, false, false);

  CMICmdArgContext context("\\\"a\\\"");
  EXPECT_FALSE(arg.Validate(context));
}

TEST(TestMICmdArgValString, SkipsSpecialContentIfDoesNotHandleQuotes) {
  CMICmdArgValString arg("arg", true, true, false);

  CMICmdArgContext context("--option i1 10 a/b c");
  EXPECT_TRUE(arg.Validate(context));
  EXPECT_EQ("c", arg.GetValue());
}

TEST(TestMICmdArgValString, DoesNotSkipSpecialContentIfHandlesQuotes) {
  CMICmdArgValString arg("arg", true, true, true);

  CMICmdArgContext context("--option c");
  EXPECT_FALSE(arg.Validate(context));

  context = CMICmdArgContext("i1 c");
  EXPECT_FALSE(arg.Validate(context));

  context = CMICmdArgContext("10 c");
  EXPECT_FALSE(arg.Validate(context));

  context = CMICmdArgContext("a/b c");
  EXPECT_FALSE(arg.Validate(context));
}

TEST(TestMICmdArgValString, HandlesPathsIfNeeded) {
  CMICmdArgValString arg("arg", true, true, true, false, true);
  CMICmdArgContext context("a/b");
  EXPECT_TRUE(arg.Validate(context));
  EXPECT_EQ("a/b", arg.GetValue());

  arg = CMICmdArgValString("arg", true, true, true, false, false);
  context = CMICmdArgContext("a/b");
  EXPECT_FALSE(arg.Validate(context));
}

TEST(TestMICmdArgValString, HandlesNumbersIfNeeded) {
  CMICmdArgValString arg("arg", true, true, true, true, false);
  CMICmdArgContext context("10");
  EXPECT_TRUE(arg.Validate(context));
  EXPECT_EQ("10", arg.GetValue());

  arg = CMICmdArgValString("arg", true, true, true, false, false);
  context = CMICmdArgContext("10");
  EXPECT_FALSE(arg.Validate(context));
}
