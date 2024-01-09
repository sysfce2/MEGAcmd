/**
 * (c) 2013 by Mega Limited, Auckland, New Zealand
 *
 * This file is part of MEGAcmd.
 *
 * MEGAcmd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "MegaCmdTestingTools.h"
#include "TestUtils.h"

class ExportTest : public NOINTERACTIVELoggedInTest {};

namespace {
    // We'll use these regex to verify the links and authentication keys are well-formed
    // Links end in <handle>#<key>, whereas tokens end in <handle>#<key>:<auth-key>
    // <handle> and <auth-key> are simply alphanumeric, and <key> can also contain '-'
    // Since GTest uses a very simple Regex implementation on Windows, we cannot use groups or brackets (see: https://google.github.io/googletest/advanced.html#regular-expression-syntax)
    const std::string megaFileLinkRegex("https:\\/\\/mega.nz\\/file\\/\\w+\\#\\S+");
    const std::string megaFolderLinkRegex("https:\\/\\/mega.nz\\/folder\\/\\w+\\#\\S+");
    const std::string authTokenRegex("\\w+\\#\\S+\\:\\w+");
}

TEST_F(ExportTest, Basic)
{
    {
        G_SUBTEST << "File";
        const std::string file_path = "file01.txt";

        auto rExport = executeInClient({"export", file_path});
        ASSERT_FALSE(rExport.ok());
        EXPECT_THAT(rExport.out(), testing::HasSubstr(file_path + " is not exported"));
        EXPECT_THAT(rExport.out(), testing::HasSubstr("Use -a to export it"));

        auto rCreate = executeInClient({"export", "-a", "-f", file_path});
        ASSERT_TRUE(rCreate.ok());
        EXPECT_THAT(rCreate.out(), testing::HasSubstr("Exported /" + file_path));
        EXPECT_THAT(rCreate.out(), testing::ContainsRegex(megaFileLinkRegex));

        // Verify it shows up as exported
        rExport = executeInClient({"export", file_path});
        ASSERT_TRUE(rExport.ok());
        EXPECT_THAT(rExport.out(), testing::HasSubstr(file_path));
        EXPECT_THAT(rExport.out(), testing::HasSubstr("shared as exported permanent file link"));
        EXPECT_THAT(rExport.out(), testing::ContainsRegex(megaFileLinkRegex));

        auto rDisable = executeInClient({"export", "-d", file_path});
        ASSERT_TRUE(rDisable.ok());
        EXPECT_THAT(rDisable.out(), testing::StartsWith("Disabled export: /" + file_path));

        // Again, verify it's not exported
        rExport = executeInClient({"export", file_path});
        ASSERT_FALSE(rExport.ok());
        EXPECT_THAT(rExport.out(), testing::HasSubstr(file_path + " is not exported"));
        EXPECT_THAT(rExport.out(), testing::HasSubstr("Use -a to export it"));
    }

    {
        G_SUBTEST << "Directory";
        const std::string dir_path = "testExportFolder";

        auto rExport = executeInClient({"export", dir_path});
        ASSERT_FALSE(rExport.ok());
        EXPECT_THAT(rExport.out(), testing::HasSubstr("Couldn't find anything exported below"));
        EXPECT_THAT(rExport.out(), testing::HasSubstr(dir_path));
        EXPECT_THAT(rExport.out(), testing::HasSubstr("Use -a to export it"));

        auto rCreate = executeInClient({"export", "-a", "-f", dir_path});
        ASSERT_TRUE(rCreate.ok());
        EXPECT_THAT(rCreate.out(), testing::HasSubstr("Exported /" + dir_path));
        EXPECT_THAT(rCreate.out(), testing::ContainsRegex(megaFolderLinkRegex));

        // Verify it shows up as exported
        rExport = executeInClient({"export", dir_path});
        ASSERT_TRUE(rExport.ok());
        EXPECT_THAT(rExport.out(), testing::HasSubstr(dir_path));
        EXPECT_THAT(rExport.out(), testing::HasSubstr("shared as exported permanent folder link"));
        EXPECT_THAT(rExport.out(), testing::ContainsRegex(megaFolderLinkRegex));

        auto rDisable = executeInClient({"export", "-d", dir_path});
        ASSERT_TRUE(rDisable.ok());
        EXPECT_THAT(rDisable.out(), testing::StartsWith("Disabled export: /" + dir_path));

        // Again, verify it's not exported
        rExport = executeInClient({"export", dir_path});
        ASSERT_FALSE(rExport.ok());
        EXPECT_THAT(rExport.out(), testing::HasSubstr("Couldn't find anything exported below"));
        EXPECT_THAT(rExport.out(), testing::HasSubstr(dir_path));
        EXPECT_THAT(rExport.out(), testing::HasSubstr("Use -a to export it"));
    }
}

TEST_F(ExportTest, FailedRecreation)
{
    const std::string file_path = "file01.txt";
    const std::vector<std::string> createCommand{"export", "-a", "-f", file_path};

    auto rCreate = executeInClient(createCommand);
    ASSERT_TRUE(rCreate.ok());
    EXPECT_THAT(rCreate.out(), testing::HasSubstr("Exported /" + file_path));
    EXPECT_THAT(rCreate.out(), testing::ContainsRegex(megaFileLinkRegex));

    rCreate = executeInClient(createCommand);
    ASSERT_FALSE(rCreate.ok());
    EXPECT_THAT(rCreate.out(), testing::HasSubstr(file_path + " is already exported"));

    auto rDisable = executeInClient({"export", "-d", file_path});
    ASSERT_TRUE(rDisable.ok());
    EXPECT_THAT(rDisable.out(), testing::StartsWith("Disabled export: /" + file_path));
}

TEST_F(ExportTest, Writable)
{
    const std::string dir_path = "testExportFolder";

    auto rOnlyWritable = executeInClient({"export", "--writable", dir_path});
    ASSERT_FALSE(rOnlyWritable.ok());
    EXPECT_THAT(rOnlyWritable.out(), testing::HasSubstr("Option can only be used when adding an export"));

    auto rCreate = executeInClient({"export", "-a", "-f", "--writable", dir_path});
    ASSERT_TRUE(rCreate.ok());
    EXPECT_THAT(rCreate.out(), testing::HasSubstr("Exported /" + dir_path));
    EXPECT_THAT(rCreate.out(), testing::ContainsRegex(megaFolderLinkRegex));
    EXPECT_THAT(rCreate.out(), testing::ContainsRegex("AuthToken = " + authTokenRegex));

    // Verify the authToken is also present when checking the export (not just when creating it)
    auto rCheck = executeInClient({"export", dir_path});
    ASSERT_TRUE(rCheck.ok());
    EXPECT_THAT(rCheck.out(), testing::HasSubstr(dir_path));
    EXPECT_THAT(rCheck.out(), testing::HasSubstr("shared as exported permanent folder link"));
    EXPECT_THAT(rCheck.out(), testing::ContainsRegex(megaFolderLinkRegex));
    EXPECT_THAT(rCheck.out(), testing::ContainsRegex("AuthToken=" + authTokenRegex));

    auto rDisable = executeInClient({"export", "-d", dir_path});
    ASSERT_TRUE(rDisable.ok());
    EXPECT_THAT(rDisable.out(), testing::StartsWith("Disabled export: /" + dir_path));
}

