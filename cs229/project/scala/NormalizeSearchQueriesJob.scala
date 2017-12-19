import com.twitter.scalding._
import com.twitter.scalding.source.TypedText
import com.twitter.scalding.typed.TypedPipe
import com.twitter.scalding_internal.job.{RequiredBinaryComparators, TwitterDateJob}

/**

Run:
  ./pants compile topics && ./pants bundle topics:topics-deploy
  oscar hdfs --screen \
    --bundle topics-deploy --job NormalizeSearchQueriesJob
  */
class NormalizeSearchQueriesJob(args : Args) extends TwitterDateJob(args) with RequiredBinaryComparators {

  val input = TypedText.tsv[(String, Int)]("/user/tstramer/searchQueries")
  val output = TypedText.tsv[(String, Int)]("/user/tstramer/normalizedSearchQueries")

  TypedPipe
    .from(input)
    .map { case (query, count) =>
      (query.toLowerCase, count)
    }
    .sumByKey
    .toTypedPipe
    .write(output)
}