-- 清理重复的插件菜单
-- 保留每个 href 和 pluginId 组合中 id 最小的记录，删除其他重复记录

DELETE FROM menu WHERE id NOT IN (
    SELECT MIN(id) FROM menu 
    WHERE pluginId IS NOT NULL 
    GROUP BY href, pluginId
) AND pluginId IS NOT NULL;
