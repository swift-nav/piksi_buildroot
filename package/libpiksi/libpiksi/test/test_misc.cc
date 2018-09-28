/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Dev <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <fcntl.h>
#include <sys/socket.h>

#include <gtest/gtest.h>

#include <libpiksi_tests.h>

#include <libpiksi/util.h>

TEST_F(LibpiksiTests, isFileTests)
{
  // File
  {
    const char *test_file = "/tmp/file";
    int fd = open(test_file, O_CREAT | O_WRONLY);
    ASSERT_FALSE(fd == -1);
    EXPECT_TRUE(is_file(fd));
    close(fd);
    // Clean up test file
    if (remove(test_file)) {
      std::cout << "Failed to clean up " << test_file << std::endl;
    }
  }

  // Pipe
  {
    int fd[2];
    ASSERT_FALSE(pipe(fd) == -1);
    EXPECT_FALSE(is_file(fd[0]));
    EXPECT_FALSE(is_file(fd[1]));
    close(fd[0]);
    close(fd[1]);
  }

  // Socket
  {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_FALSE(fd == -1);
    EXPECT_FALSE(is_file(fd));
    close(fd);
  }

  // FIFO
  {
    const char *myfifo = "/tmp/myfifo";
    ASSERT_FALSE(mkfifo(myfifo, 0777) == -1);
    int fd = open(myfifo, O_RDONLY | O_NONBLOCK);
    ASSERT_FALSE(fd == -1);
    EXPECT_FALSE(is_file(fd));
    close(fd);
    // Clean up FIFO
    if (unlink(myfifo)) {
      std::cout << "Failed to clean up " << myfifo << std::endl;
    }
  }
}
