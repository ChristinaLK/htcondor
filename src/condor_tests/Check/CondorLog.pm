##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************

package CondorLog;

use CondorTest;

sub RunCheck
{
    my %args = @_;

    my $daemon = $args{daemon} || die("'daemon' not specified");
    my $match_regexp = $args{match_regexp} || die("'match_regexp' not specified");
    my $fail_if_found = $args{fail_if_found} || 0;
    my $num_retries = $args{num_retries} || 0;

    my $result;
    my $count = 0;
    while(1) {
	$result = CondorTest::SearchCondorLog($daemon,$match_regexp);
	
	last if $result;
	last if ($count >= $num_retries);
	sleep(1);
    }

    if( $fail_if_found ) {
	$result = !$result;
    }

    CondorTest::RegisterResult( $result, %args );
    return $result;
}

sub RunCheckMultiple
{
    my %args = @_;

    my $daemon = $args{daemon} || die("'daemon' not specified");
    my $match_regexp = $args{match_regexp} || die("'match_regexp' not specified");
	my $match_instances = $args{match_instances} || 1;
    my $match_timeout = $args{match_timeout} || 10;
	my $match_new = $args{match_new} || "false";

    my $result;
    my $count = 0;
	$result = CondorTest::SearchCondorLogMultiple($daemon,$match_regexp,$match_instances,$match_timeout,$match_new);

    CondorTest::RegisterResult( $result, %args );
    return $result;
}

sub RunSpecialCheck
{
    my %args = @_;

    my $logname = $args{logname} || die("'logname' not specified");
    my $match_regexp = $args{match_regexp} || die("'match_regexp' not specified");
    my $allmatch = $args{allmatch} || 0;
    my $num_retries = $args{num_retries} || 0;

    my $result;
    my $count = 0;
    while(1) {
	$result = CondorTest::SearchCondorSpecialLog($logname,$match_regexp,$allmatch);
	
	last if $result;
	last if ($count >= $num_retries);
	sleep(1);
    }

    if( $fail_if_found ) {
	$result = !$result;
    }

    CondorTest::RegisterResult( $result, %args );
    return $result;
}

1;
