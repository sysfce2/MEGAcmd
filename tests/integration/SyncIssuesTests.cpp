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

#include <filesystem>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "megacmdcommonutils.h"
#include "TestUtils.h"
#include "MegaCmdTestingTools.h"

using TI = TestInstruments;

// This class ensures that the sync issue list eventually
// reaches the expected size (before a certain timeout)
class SyncIssueListGuard
{
    uint64_t mCurrentListSize;
    const uint64_t mExpectedListSize;
    bool mTriggered;

    std::mutex mMtx;
    std::condition_variable mCv;

public:
    SyncIssueListGuard(uint64_t expectedListSize) :
        mCurrentListSize(0),
        mExpectedListSize(expectedListSize),
        mTriggered(false)
    {
        TI::Instance().onEveryEvent(TI::Event::SYNC_ISSUES_LIST_UPDATED, [this]
        {
            auto syncIssueListSizeOpt = TI::Instance().testValue(TI::TestValue::SYNC_ISSUES_LIST_SIZE);
            EXPECT_TRUE(syncIssueListSizeOpt.has_value());

            {
                std::lock_guard guard(mMtx);
                mCurrentListSize = std::get<uint64_t>(*syncIssueListSizeOpt);
                mTriggered = true;
            }
            mCv.notify_one();
        });
    }

    ~SyncIssueListGuard()
    {
        {
            std::unique_lock lock(mMtx);
            mCv.wait_for(lock, std::chrono::seconds(5), [this] { return mTriggered && mCurrentListSize == mExpectedListSize; });

            EXPECT_EQ(mCurrentListSize, mExpectedListSize);
        }

        TI::Instance().clearEvent(TI::Event::SYNC_ISSUES_LIST_UPDATED);
        TI::Instance().resetTestValue(TI::TestValue::SYNC_ISSUES_LIST_SIZE);
    }
};

template<bool runSync>
class SyncIssuesTests_ : public NOINTERACTIVELoggedInTest
{
    SelfDeletingTmpFolder mTmpDir;

    void SetUp() override
    {
        NOINTERACTIVELoggedInTest::SetUp();
        TearDown();

        auto result = executeInClient({"mkdir", "-p", syncDirCloud()}).ok();
        ASSERT_TRUE(result);

        result = fs::create_directory(syncDirLocal());
        ASSERT_TRUE(result);

        if constexpr (runSync)
        {
            SyncIssueListGuard guard(0); // ensure there are no sync issues before we start the test
            result = executeInClient({"sync", syncDirLocal(), syncDirCloud()}).ok();
            ASSERT_TRUE(result);
        }
    }

    void TearDown() override
    {
        removeAllSyncs();

        fs::remove_all(syncDirLocal());
        executeInClient({"rm", "-r", "-f", syncDirCloud()});
    }

protected:
    std::string syncDirLocal() const
    {
        return mTmpDir.string() + "/tests_integration_sync_dir/";
    }

    std::string syncDirCloud() const
    {
        return "tests_integration_sync_dir/";
    }

    std::string qw(const std::string& str) // quote wrap
    {
        return "'" + str + "'";
    }
};

using SyncIssuesTests = SyncIssuesTests_<true>;
using ManualSyncIssuesTests = SyncIssuesTests_<false>;

TEST_F(SyncIssuesTests, NoIssues)
{
    auto result = executeInClient({"sync-issues"});
    ASSERT_TRUE(result.ok());
    EXPECT_THAT(result.out(), testing::HasSubstr("There are no sync issues"));
}

TEST_F(SyncIssuesTests, NameConflict)
{
#ifdef _WIN32
    const char* conflictingName = "F01";
#else
    const char* conflictingName = "f%301";
#endif

    auto rMkdir = executeInClient({"mkdir", syncDirCloud() + "f01"});
    ASSERT_TRUE(rMkdir.ok());

    {
        // Register the event callback *before* causing the sync issue
        SyncIssueListGuard guard(1);

        // Cause the name conclict
        rMkdir = executeInClient({"mkdir", syncDirCloud() + conflictingName});
        ASSERT_TRUE(rMkdir.ok());
    }

    // Check the name conclict appears in the list of sync issues
    auto result = executeInClient({"sync-issues", "--disable-path-collapse"});
    ASSERT_TRUE(result.ok());
    EXPECT_THAT(result.out(), testing::Not(testing::HasSubstr("There are no sync issues")));
    EXPECT_THAT(result.out(), testing::AnyOf(testing::HasSubstr(qw("f01")), testing::HasSubstr(qw(conflictingName))));
}

TEST_F(SyncIssuesTests, SymLink)
{
    const std::string dirPath = syncDirLocal() + "some_dir";
    ASSERT_TRUE(fs::create_directory(dirPath));

    std::string linkName = "some_link";
    std::string linkPath = syncDirLocal() + linkName;

#ifdef _WIN32
    megacmd::replaceAll(linkPath, "/", "\\");
#endif

    {
        SyncIssueListGuard guard(1);
        fs::create_directory_symlink(dirPath, linkPath);
    }

    // Check the symlink in the list of sync issues
    auto result = executeInClient({"sync-issues", "--disable-path-collapse"});
    ASSERT_TRUE(result.ok());
    EXPECT_THAT(result.out(), testing::Not(testing::HasSubstr("There are no sync-issues issues")));
    EXPECT_THAT(result.out(), testing::HasSubstr(qw(linkName)));

    {
        SyncIssueListGuard guard(0);
        fs::remove(linkPath);
    }

    result = executeInClient({"sync-issues"});
    ASSERT_TRUE(result.ok());
    EXPECT_THAT(result.out(), testing::HasSubstr("There are no sync issues"));
}

TEST_F(ManualSyncIssuesTests, FileAgainstFolder)
{
    const std::string fileName = "fake_file";

    auto result = executeInClient({"mkdir", "-p", syncDirCloud() + fileName});
    ASSERT_TRUE(result.ok());

    {
        std::ofstream file(syncDirLocal() + fileName);
        file << "Some data";
    }

    // Start syncing once we've setup inconsistent data in cloud and local
    {
        SyncIssueListGuard guard(1);
        result = executeInClient({"sync", syncDirLocal(), syncDirCloud()});
        ASSERT_TRUE(result.ok());
    }

    result = executeInClient({"sync-issues", "--disable-path-collapse", "--detail", "--all"});
    ASSERT_TRUE(result.ok());

    auto lines = splitByNewline(result.out());
    ASSERT_THAT(lines, testing::Contains(testing::HasSubstr("Unable to sync " + qw(fileName))));
    ASSERT_THAT(lines, testing::Contains(testing::HasSubstr("Cannot sync folders against files")));
    ASSERT_THAT(lines, testing::Contains(testing::AllOf(testing::HasSubstr("<CLOUD>"), testing::HasSubstr("Folder"))));
    ASSERT_THAT(lines, testing::Contains(testing::AllOf(testing::Not(testing::HasSubstr("<CLOUD>")), testing::HasSubstr("File"))));
}

TEST_F(SyncIssuesTests, IncorrectSyncIssueListSizeOnSecondSymlink)
{
    // This tests against an internal issue that caused the sync issue list to have incorrect
    // size for a brief period of time after creating a second symlink. This is unlikely to
    // affect how users interact with MEGAcmd, since it happened over a short timespan. But the test
    // is still useful to prove the internal correctness of our sync issues is not broken.
    // Note: required sdk fix (+our changes) to pass (see: SDK-4016)

    const std::string dirPath = syncDirLocal() + "some_dir";
    ASSERT_TRUE(fs::create_directory(dirPath));

    // The first link doesn't cause an issue, but we still need to wait for it
    {
        SyncIssueListGuard guard(1);
        fs::create_directory_symlink(dirPath, syncDirLocal() + "link1");
    }

    // The second link causes the issue
    {
        SyncIssueListGuard guard(2);
        fs::create_directory_symlink(dirPath, syncDirLocal() + "link2");
    }
}

TEST_F(SyncIssuesTests, LimitedSyncIssueList)
{
    const std::string dirPath = syncDirLocal() + "some_dir";
    ASSERT_TRUE(fs::create_directory(dirPath));

    // Create 5 sync issues
    for (int i = 1; i <= 5; ++i)
    {
        SyncIssueListGuard guard(i);
        fs::create_directory_symlink(dirPath, syncDirLocal() + "link" + std::to_string(i));
    }

    auto result = executeInClient({"sync-issues", "--limit=" + std::to_string(3)});
    ASSERT_TRUE(result.ok());

    // There should be 7 lines (column header + limit=3 + newline + detail usage + limit-specific note)
    auto lines = splitByNewline(result.out());
    EXPECT_THAT(lines, testing::SizeIs(7));
    EXPECT_THAT(lines.at(lines.size()-2), testing::HasSubstr("showing 3 out of 5 issues"));
}

TEST_F(SyncIssuesTests, ShowSyncIssuesInSyncCommand)
{
    auto result = executeInClient({"sync"});
    ASSERT_TRUE(result.ok());
    EXPECT_THAT(result.out(), testing::HasSubstr(" NO "));
    EXPECT_THAT(result.out(), testing::Not(testing::HasSubstr("You have sync issues")));

    const std::string dirPath = syncDirLocal() + "some_dir";
    ASSERT_TRUE(fs::create_directory(dirPath));

    {
        SyncIssueListGuard guard(1);
        fs::create_directory_symlink(dirPath, syncDirLocal() + "link1");
    }
    result = executeInClient({"sync"});
    ASSERT_TRUE(result.ok());
    EXPECT_THAT(result.out(), testing::HasSubstr("Sync Issues (1)"));
    EXPECT_THAT(result.out(), testing::Not(testing::HasSubstr(" NO ")));
    EXPECT_THAT(result.err(), testing::HasSubstr("You have sync issues"));

    {
        SyncIssueListGuard guard(2);
        fs::create_directory_symlink(dirPath, syncDirLocal() + "link2");
    }
    result = executeInClient({"sync"});
    ASSERT_TRUE(result.ok());
    EXPECT_THAT(result.out(), testing::HasSubstr("Sync Issues (2)"));
    EXPECT_THAT(result.out(), testing::Not(testing::HasSubstr(" NO ")));
    EXPECT_THAT(result.err(), testing::HasSubstr("You have sync issues"));
}

TEST_F(SyncIssuesTests, SyncIssueDetail)
{
    const std::string dirPath = syncDirLocal() + "some_dir";
    ASSERT_TRUE(fs::create_directory(dirPath));

    std::string linkPath = syncDirLocal() + "some_link";
#ifdef _WIN32
    megacmd::replaceAll(linkPath, "/", "\\");
#endif

    {
        SyncIssueListGuard guard(1);
        fs::create_directory_symlink(dirPath, linkPath);
    }

    // Get the sync issue id
    auto result = executeInClient({"sync-issues"});
    ASSERT_TRUE(result.ok());

    auto lines = splitByNewline(result.out());
    EXPECT_EQ(lines.size(), 4); // Column names + issue + newline + detail usage

    auto words = megacmd::split(lines[1], " ");
    EXPECT_THAT(words, testing::Not(testing::IsEmpty()));

    std::string syncIssueId = words[0];

    // Get the parent sync ID
    result = executeInClient({"sync"});
    ASSERT_TRUE(result.ok());

    lines = splitByNewline(result.out());
    EXPECT_THAT(lines.size(), 3);

    words = megacmd::split(lines[1], " ");
    EXPECT_THAT(words, testing::Not(testing::IsEmpty()));

    std::string parentSyncId = words[0];

    result = executeInClient({"sync-issues", "--disable-path-collapse", "--detail", "--", syncIssueId});
    ASSERT_TRUE(result.ok());
    EXPECT_THAT(result.out(), testing::HasSubstr("Parent sync: " + parentSyncId));
    EXPECT_THAT(result.out(), testing::HasSubstr("Symlink detected"));
    EXPECT_THAT(result.out(), testing::HasSubstr(linkPath));
}

TEST_F(SyncIssuesTests, AllSyncIssuesDetail)
{
    const std::string dirPath = syncDirLocal() + "some_dir";
    ASSERT_TRUE(fs::create_directory(dirPath));

    std::string linkPath = syncDirLocal() + "some_link";
#ifdef _WIN32
    megacmd::replaceAll(linkPath, "/", "\\");
#endif

    {
        SyncIssueListGuard guard(1);
        fs::create_directory_symlink(dirPath, linkPath);
    }

    // Get the sync issue id
    auto result = executeInClient({"sync-issues"});
    ASSERT_TRUE(result.ok());

    auto lines = splitByNewline(result.out());
    EXPECT_EQ(lines.size(), 4); // Column names + issue + newline + detail usage

    auto words = megacmd::split(lines[1], " ");
    EXPECT_THAT(words, testing::Not(testing::IsEmpty()));

    std::string syncIssueId = words[0];

    // Get the parent sync ID
    result = executeInClient({"sync"});
    ASSERT_TRUE(result.ok());

    lines = splitByNewline(result.out());
    EXPECT_THAT(lines.size(), 3);

    words = megacmd::split(lines[1], " ");
    EXPECT_THAT(words, testing::Not(testing::IsEmpty()));

    std::string parentSyncId = words[0];

    result = executeInClient({"sync-issues", "--disable-path-collapse", "--detail", "--all"});
    ASSERT_TRUE(result.ok());
    EXPECT_THAT(result.out(), testing::HasSubstr("Parent sync: " + parentSyncId));
    EXPECT_THAT(result.out(), testing::HasSubstr("Symlink detected"));
    EXPECT_THAT(result.out(), testing::HasSubstr(linkPath));
    EXPECT_THAT(result.out(), testing::HasSubstr("Details on issue " + syncIssueId));
}

TEST_F(ManualSyncIssuesTests, AllSyncIssuesDetailEnforceReasonsAndPathProblems)
{
    // Configure a conflicting issue and iterate over possible sync issues reasons and path problems,
    // to exercise code paths for all:
    const std::string fileName = "fake_file";

    auto result = executeInClient({"mkdir", "-p", syncDirCloud() + fileName});
    ASSERT_TRUE(result.ok());

    {
        std::ofstream file(syncDirLocal() + fileName);
        file << "Some data";
    }

    // Start syncing once we've setup inconsistent data in cloud and local
    {
        SyncIssueListGuard guard(1);
        result = executeInClient({"sync", syncDirLocal(), syncDirCloud()});
        ASSERT_TRUE(result.ok());
    }

    // Get the sync issue id
    result = executeInClient({"sync-issues"});
    ASSERT_TRUE(result.ok());

    auto lines = splitByNewline(result.out());
    EXPECT_EQ(lines.size(), 4); // Column names + issue + newline + detail usage

    auto words = megacmd::split(lines[1], " ");
    EXPECT_THAT(words, testing::Not(testing::IsEmpty()));

    std::string syncIssueId = words[0];

    // Get the parent sync ID
    result = executeInClient({"sync"});
    ASSERT_TRUE(result.ok());

    lines = splitByNewline(result.out());
    EXPECT_THAT(lines.size(), 3);

    words = megacmd::split(lines[1], " ");
    EXPECT_THAT(words, testing::Not(testing::IsEmpty()));

    std::string parentSyncId = words[0];

    for (int reasonType = static_cast<int>(mega::SyncWaitReason::NoReason);
         reasonType < static_cast<int>(mega::SyncWaitReason::SyncWaitReason_LastPlusOne);
         ++reasonType)
    {
        for (int pathProblem = static_cast<int>(mega::PathProblem::NoProblem);
             pathProblem < static_cast<int>(mega::PathProblem::PathProblem_LastPlusOne);
             ++pathProblem)
        {
            if (pathProblem == 19)
            {
                continue; // deprecated
            }

            TestInstrumentsTestValueGuard reasonGuard(TI::TestValue::SYNC_ISSUE_ENFORCE_REASON_TYPE, (int64_t) reasonType);
            TestInstrumentsTestValueGuard pathProblemGuard(TI::TestValue::SYNC_ISSUE_ENFORCE_PATH_PROBLEM, (int64_t) pathProblem);

            result = executeInClient({"sync-issues", "--disable-path-collapse", "--detail", "--all"});

            ASSERT_TRUE(result.ok());
            EXPECT_THAT(result.out(), testing::HasSubstr("Parent sync: " + parentSyncId));
            EXPECT_THAT(result.out(), testing::HasSubstr("Details on issue " + syncIssueId));
        }
    }
}
