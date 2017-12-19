import com.twitter.common.text.pipeline.TwitterLanguageIdentifier
import com.twitter.pluck.source.StatusSource
import com.twitter.scalding._
import com.twitter.scalding.source.TypedText
import com.twitter.scalding.typed.TypedPipe
import com.twitter.scalding_internal.job.{TwitterJob, RequiredBinaryComparators, TwitterDateJob}
import java.util.Locale
import com.twitter.common.text.pipeline.TwitterLanguageIdentifier
import com.twitter.pluck.source.StatusSource
import com.twitter.pluck.source.combined_user_source.MostRecentCombinedUserSnapshotSource
import com.twitter.scalding._
import com.twitter.scalding.source.TypedText
import com.twitter.scalding.typed.TypedPipe
import com.twitter.scalding_internal.job.{TwitterJob, RequiredBinaryComparators, TwitterDateJob}
import java.util.Locale
import scala.util.Random

/**

Run:
  ./pants compile topics && ./pants bundle topics:topics-deploy
  oscar hdfs --screen \
    --bundle topics-deploy --job UserRowsToCountsMatrixJob \
    -- \
    --output userMatrix

To see the data:
  science/scalding/scripts/hdfsget userMatrix
  cat userMatrix
  */
class UserRowsToCountsMatrixJob(args : Args) extends TwitterJob(args) with RequiredBinaryComparators {

  val input = TypedText.tsv[UserRowDetailed.Tuple]("/user/tstramer/usersToQueryCountsWithMetadata.tsv")

  TypedPipe.from(input)
    .map(UserRowDetailed.fromTuple)
    .map { userRow =>
      (SearchQueries.All -- SearchQueries.Filtered).toSeq.sorted.map { query =>
        userRow.counts.getOrElse(query, 0)
      }.mkString("\t")
    }
    // write the output to a TypedText.tsv
    .write(TypedText.tsv[String](args("output")))
}