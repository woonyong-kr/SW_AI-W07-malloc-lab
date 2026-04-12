#!/usr/bin/perl
#!/usr/local/bin/perl

$out_filename = $argv[0];
$out_filename = "random.rep" unless $out_filename;
$num_blocks = $argv[1];
# $num_blocks = 1200 unless $num_blocks;
$num_blocks = 2400 unless $num_blocks;
$max_blk_size = $argv[2];
$max_blk_size = 32768 unless $max_blk_size;

#print "출력 파일: $out_filename\n";
#print "블록 개수: $num_blocks\n";
#print "최대 블록 크기: $max_blk_size\n";

# trace를 생성한다
# malloc() 연산들을 연속해서 만든다
for ($i = 0;  $i < $num_blocks; $i += 1) {
    $size = int(rand $max_blk_size);
    $op = {};
    $op->{type} = "a";
    $op->{seq} = $i;
    $op->{size} = $size;
    $total_block_size += $size;
    push @trace, $op;
}
# 적절한 위치에 free()를 삽입한다
for ($i = 0;  $i < $num_blocks; $i += 1) {
    for ($minval = $i; $minval < $num_blocks + $i; $minval += 1) {
        if (($trace[$minval]->{type} eq "a") && ($trace[$minval]->{seq} == $i)) {
            last;
        }
    }
    $pos = int(rand($num_blocks + $i - $minval - 1) + $minval + 1);
    $op = {};
    $op->{type} = "f";
    $op->{seq} = $i;
    splice @trace, $pos, 0, $op;
}

# 출력 파일을 연다
open OUTFILE, ">$out_filename" or die "Cannot create $out_filename\n";

# 기타 파라미터를 계산한다
$suggested_heap_size = $total_block_size + 100;
$num_ops = 2*$num_blocks;

print OUTFILE "$suggested_heap_size\n";
print OUTFILE "$num_blocks\n";
print OUTFILE "$num_ops\n";
print OUTFILE "1\n";

for ($i = 0;  $i < 2*$num_blocks; $i += 1) {
    if ($trace[$i]->{type} eq "a") {
        print OUTFILE "$trace[$i]->{type} $trace[$i]->{seq} $trace[$i]->{size}\n";
    } else {
        print OUTFILE "$trace[$i]->{type} $trace[$i]->{seq}\n";
    }
}

close OUTFILE;
