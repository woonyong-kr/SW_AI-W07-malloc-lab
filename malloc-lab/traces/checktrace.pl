#!/usr/bin/perl 
#!/usr/local/bin/perl 
use Getopt::Std;

#######################################################################
# checktrace - trace 파일의 일관성을 검사하고 균형을 맞추는 도구
#
# Copyright (c) 2002, R. Bryant와 D. O'Hallaron, 모든 권리 보유.
# 허가 없이 사용, 수정, 복사할 수 없다.
#
# 이 스크립트는 Malloc Lab trace 파일을 읽어 일관성을 검사하고,
# 필요한 free 요청을 덧붙여 균형 잡힌 버전을 출력한다.
#
#######################################################################
 
$| = 1; # 각 print 문마다 출력을 즉시 플러시한다

#
# void usage(void) - 도움말을 출력하고 종료한다
#
sub usage 
{
    printf STDERR "$_[0]\n";
    printf STDERR "Usage: $0 [-hs]\n";
    printf STDERR "Options:\n";
    printf STDERR "  -h          Print this message\n";
    printf STDERR "  -s          Emit only a brief summary\n";
    die "\n" ;
}

##############
# 메인 루틴
##############

# 
# 명령줄 인자를 파싱하고 검사한다
#
getopts('hs');
if ($opt_h) {
    usage("");
}
$summary = $opt_s;

# 
# HASH는 아직 짝이 맞지 않은 alloc/realloc 요청을 계속 기록한다.
# free 요청을 만나면 해당 해시 항목을 삭제한다.
# trace를 모두 읽고 난 뒤 남아 있는 항목은 짝이 맞지 않는
# alloc/realloc 요청이다.
#
%HASH = (); 

# trace 헤더 값을 읽는다
$heap_size = <STDIN>;
chomp($heap_size);

$num_blocks = <STDIN>;
chomp($num_blocks);

$old_num_ops = <STDIN>;
chomp($old_num_ops);

$weight = <STDIN>;
chomp($weight);

# 
# 대응되는 free 요청이 없는 할당 요청을 찾는다
#
$linenum = 4;
$requestnum = 0;
while ($line = <STDIN>) {
    chomp($line);
    $linenum++;

    ($cmd, $id, $size) = split(" ", $line);

    # 빈 줄은 무시한다
    if (!$cmd) {
	next;
    }

    # 나중에 출력할 수 있도록 현재 줄을 저장한다
    $lines[$requestnum++] = $line;

    # 이전에 alloc 요청이 있었다면 realloc 요청은 그대로 통과시킨다
    if ($cmd eq "r") {
	if (!$HASH{$id}) {
	    die "$0: ERROR[$linenum]: realloc without previous alloc\n";
	}
	next;
    }

    if ($cmd eq "a" and $HASH{$id} eq "a") {
	die "$0: ERROR[$linenum]: allocate with no intervening free.\n";
    }

    if ($cmd eq "a" and $HASH{$id} eq "f") {
	die "$0: ERROR[$linenum]: reused ID $id.\n";
    }

    if ($cmd eq "f" and !exists($HASH{$id})) {
	die "$0: ERROR[$linenum]: freeing unallocated block.\n";
	next;
    }

    if ($cmd eq "f" and !$HASH{$id} eq "f") {
	die "$0: ERROR[$linenum]: freeing already freed block.\n";
	next;
    }
    
    if ($cmd eq "f") {
	delete $HASH{$id};
    }
    else {
	$HASH{$id} = $cmd;
    }
}

# 
# -s 인자와 함께 호출되면 간단한 균형 요약만 출력하고 종료한다
#
if ($summary) {
    if (!%HASH) {
	print "Balanced trace.\n";
    } 
    else {
	print "Unbalanced tree.\n";
    }
    exit;
}

#
# 균형이 맞는 trace 버전을 출력한다
#
$new_ops = keys %HASH;
$new_num_ops = $old_num_ops + $new_ops;

print "$heap_size\n";
print "$num_blocks\n";
print "$new_num_ops\n";
print "$weight\n";

# 기존 요청을 출력한다
foreach $item (@lines) {
    print "$item\n";
}

# trace의 균형을 맞출 free 요청 집합을 출력한다
foreach $key (sort keys %HASH) {
    if ($HASH{$key} ne "a" and $HASH{$key} ne "r") {
	die "$0: ERROR: Invalid free request in residue.\n";
    }
    print "f $key\n";
}

exit;
