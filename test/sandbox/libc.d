/* http://docs.oracle.com/cd/E18752_01/html/819-5488/gcgkk.html#gcglh */

/* pid$target::free: */

/* Trace entries into all functions */
pid$target:::entry
{
    @[probefunc] = count();
}
